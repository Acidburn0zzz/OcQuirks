#include <Library/OcStorageLib.h>
#include <Library/OcSerializeLib.h>
#include <Library/OcTemplateLib.h>
#include <Library/OcAppleBootCompatLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/OcConsoleLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#define ROOT_PATH   L"EFI\\CLOVER"
#define CONFIG_PATH L"drivers\\UEFI\\OcQuirks.plist"

#define MAX_DATA_SIZE 10000

#define OC_QUIRKS_FIELDS(_, __) \
  _(BOOLEAN , AvoidRuntimeDefrag      ,   , TRUE  ,()) \
  _(BOOLEAN , DevirtualiseMmio        ,   , FALSE ,()) \
  _(BOOLEAN , DisableSingleUser       ,   , FALSE ,()) \
  _(BOOLEAN , DisableVariableWrite    ,   , FALSE ,()) \
  _(BOOLEAN , DiscardHibernateMap     ,   , FALSE ,()) \
  _(BOOLEAN , EnableSafeModeSlide     ,   , TRUE  ,()) \
  _(BOOLEAN , EnableWriteUnprotector  ,   , TRUE  ,()) \
  _(BOOLEAN , ForceExitBootServices   ,   , TRUE  ,()) \
  _(BOOLEAN , ProtectCsmRegion        ,   , FALSE ,()) \
  _(BOOLEAN , ProvideConsoleGopEnable ,   , TRUE  ,()) \
  _(BOOLEAN , ProvideCustomSlide      ,   , TRUE  ,()) \
  _(BOOLEAN , SetupVirtualMap         ,   , TRUE  ,()) \
  _(BOOLEAN , ShrinkMemoryMap         ,   , FALSE ,()) \
  _(BOOLEAN , SignalAppleOS           ,   , FALSE ,())
  OC_DECLARE (OC_QUIRKS)

OC_STRUCTORS (OC_QUIRKS, ())

STATIC
OC_SCHEMA
mConfigNodes[] = {
  OC_SCHEMA_BOOLEAN_IN ("AvoidRuntimeDefrag"      , OC_QUIRKS, AvoidRuntimeDefrag),
  OC_SCHEMA_BOOLEAN_IN ("DevirtualiseMmio"        , OC_QUIRKS, DevirtualiseMmio),
  OC_SCHEMA_BOOLEAN_IN ("DisableSingleUser"       , OC_QUIRKS, DisableSingleUser),
  OC_SCHEMA_BOOLEAN_IN ("DisableVariableWrite"    , OC_QUIRKS, DisableVariableWrite),
  OC_SCHEMA_BOOLEAN_IN ("DiscardHibernateMap"     , OC_QUIRKS, DiscardHibernateMap),
  OC_SCHEMA_BOOLEAN_IN ("EnableSafeModeSlide"     , OC_QUIRKS, EnableSafeModeSlide),
  OC_SCHEMA_BOOLEAN_IN ("EnableWriteUnprotector"  , OC_QUIRKS, EnableWriteUnprotector),
  OC_SCHEMA_BOOLEAN_IN ("ForceExitBootServices"   , OC_QUIRKS, ForceExitBootServices),
  OC_SCHEMA_BOOLEAN_IN ("ProtectCsmRegion"        , OC_QUIRKS, ProtectCsmRegion),
  OC_SCHEMA_BOOLEAN_IN ("ProvideConsoleGopEnable" , OC_QUIRKS, ProvideConsoleGopEnable),
  OC_SCHEMA_BOOLEAN_IN ("ProvideCustomSlide"      , OC_QUIRKS, ProvideCustomSlide),
  OC_SCHEMA_BOOLEAN_IN ("SetupVirtualMap"         , OC_QUIRKS, SetupVirtualMap),
  OC_SCHEMA_BOOLEAN_IN ("ShrinkMemoryMap"         , OC_QUIRKS, ShrinkMemoryMap),
  OC_SCHEMA_BOOLEAN_IN ("SignalAppleOS"           , OC_QUIRKS, SignalAppleOS)
};

STATIC
OC_SCHEMA_INFO
mConfigInfo = {
  .Dict = {mConfigNodes, ARRAY_SIZE (mConfigNodes)}
};

STATIC
BOOLEAN
QuirksProvideConfig (
  OUT OC_QUIRKS *Config,
  IN EFI_HANDLE Handle
  )
{
  EFI_STATUS                        Status;
  EFI_LOADED_IMAGE_PROTOCOL         *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *FileSystem;
  OC_STORAGE_CONTEXT                Storage;
  CHAR8                             *ConfigData;
  UINT32                            ConfigDataSize;
  
  // Load SimpleFileSystem Protocol
  Status = gBS->HandleProtocol (
    Handle,
    &gEfiLoadedImageProtocolGuid,
    (VOID **) &LoadedImage
    );
  
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  
  FileSystem = LocateFileSystem (
    LoadedImage->DeviceHandle,
    LoadedImage->FilePath
    );
  
  if (FileSystem == NULL) {
    return FALSE;
  }
  
  // Init OcStorage as it already handles
  // reading Unicode files
  Status = OcStorageInitFromFs (
    &Storage,
    FileSystem,
    ROOT_PATH,
    NULL
    );
  
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  
  ConfigData = OcStorageReadFileUnicode (
    &Storage,
    CONFIG_PATH,
    &ConfigDataSize
    );
  
  // If no config data or greater than max size, fail and use defaults
  if(!ConfigDataSize || ConfigDataSize > MAX_DATA_SIZE) {
    return FALSE;
  }
  
  BOOLEAN Success = ParseSerialized (Config, &mConfigInfo, ConfigData, ConfigDataSize);
  
  gBS->FreePool(ConfigData);
  gBS->FreePool(&ConfigDataSize);
  gBS->FreePool(&Status);
  gBS->FreePool(&Storage);
  
  return Success;
}

EFI_STATUS
EFIAPI
QuirksEntryPoint (
  IN EFI_HANDLE        Handle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  OC_QUIRKS Config;
  
  OC_QUIRKS_CONSTRUCT (&Config, sizeof (Config));
  QuirksProvideConfig(&Config, Handle);
  
  OC_ABC_SETTINGS AbcSettings = {
  
    .AvoidRuntimeDefrag	    = Config.AvoidRuntimeDefrag,
    .SetupVirtualMap        = Config.SetupVirtualMap,
    .ProvideCustomSlide     = Config.ProvideCustomSlide,
    .DevirtualiseMmio       = Config.DevirtualiseMmio,
    .DisableSingleUser      = Config.DisableSingleUser,
    .DiscardHibernateMap    = Config.DiscardHibernateMap,
    .EnableSafeModeSlide    = Config.EnableSafeModeSlide,
    .ProtectCsmRegion       = Config.ProtectCsmRegion,
    .ShrinkMemoryMap        = Config.ShrinkMemoryMap,
    .ForceExitBootServices  = Config.ForceExitBootServices,
    .DisableVariableWrite   = Config.DisableVariableWrite,
    .EnableWriteUnprotector = Config.EnableWriteUnprotector
  };
  
  if (Config.ProvideConsoleGopEnable) {
  	OcProvideConsoleGop ();
  }
  
  OC_QUIRKS_DESTRUCT (&Config, sizeof (Config));
  
  return OcAbcInitialize (&AbcSettings);
}
