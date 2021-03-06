; This configuration file was generated by CMake
#define OutputFilename "${CPACK_PACKAGE_FILE_NAME}"

; Mozart config
#define MozartFolder "${CMAKE_INSTALL_PREFIX}"
#define MozartVersion "${MOZART_PROP_OZ_VERSION}"
#define LicenseFile "${CMAKE_SOURCE_DIR}/LICENSE.txt"

; Tcl/Tk config
#define NeededTclVersion "'${ISS_TCL_VERSION}'"
#define TclFolder "${ISS_TCL_PATH}"
#define TclIncluded "${ISS_INCLUDE_TCL}"

; Emacs config
#define EmacsFolder "${ISS_EMACS_PATH}"
#define EmacsIncluded "${ISS_INCLUDE_EMACS}"

; Target architecture can be x86 and/or x64
#define TargetArch "${MOZART_PROP_PLATFORM_ARCH}"
