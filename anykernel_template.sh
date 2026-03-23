### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=Realme_UI_KSU_RMX3031
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=RMX3031
device.name2=RealmeX7Max
device.name3=cupida
device.name4=mt6893
device.name5=DN2101
device.name6=DN2103
device.name7=denniz
device.name8=RMX3350
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

ui_print " ";
ui_print "************************************";
ui_print "        KSU_Next_Cupida_Denniz          ";
ui_print "************************************";
ui_print " ";
ui_print "  Kernel Source: ManshuTyagi";
ui_print "  Root Implementation: xCaptaiN09";
ui_print "  KernelSU Next + SuSFS v1.5.5";
ui_print " ";
ui_print "************************************";
ui_print " ";

# boot install
split_boot;

flash_boot;
## end boot install