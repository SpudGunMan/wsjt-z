get_target_property (QtCore_location Qt5::Core LOCATION)
get_filename_component (QtCore_location ${QtCore_location} PATH)
list (APPEND fixup_library_dirs ${QtCore_location})

if (APPLE OR WIN32)
  # install rules for including 3rd party libs such as Qt

  # install a qt.conf file
  install (CODE "
        get_filename_component (the_qt_conf \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${WSJT_QT_CONF_DESTINATION}/qt.conf\" REALPATH)
        file (WRITE \"\${the_qt_conf}\"
\"[Paths]
\")
"
    #COMPONENT runtime
    )

  # if a system Qt is used (e.g. installed in /usr/lib/), it will not be included in the installation
  set (fixup_exe "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${CMAKE_PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")

  #get_filename_component (hamlib_lib_dir ${Hamlib_LIBRARIES} PATH)

  if (APPLE)
    # Homebrew Qt plugins (e.g. imageformats) may depend on dylibs referenced as
    # @rpath/libwebp*.dylib and @rpath/libsharpyuv*.dylib. Add their directories
    # so BundleUtilities can resolve and copy them during fixup_bundle.
    find_library (WEBP_LIBRARY NAMES webp libwebp)
    if (WEBP_LIBRARY)
      get_filename_component (webp_lib_dir ${WEBP_LIBRARY} PATH)
      list (APPEND fixup_library_dirs ${webp_lib_dir})
    endif ()

    find_library (SHARPYUV_LIBRARY NAMES sharpyuv libsharpyuv)
    if (SHARPYUV_LIBRARY)
      get_filename_component (sharpyuv_lib_dir ${SHARPYUV_LIBRARY} PATH)
      list (APPEND fixup_library_dirs ${sharpyuv_lib_dir})
    endif ()

    # Fallback well-known Homebrew library locations for both Apple Silicon and Intel.
    if (EXISTS "/opt/homebrew/lib")
      list (APPEND fixup_library_dirs "/opt/homebrew/lib")
    endif ()
    if (EXISTS "/usr/local/lib")
      list (APPEND fixup_library_dirs "/usr/local/lib")
    endif ()

    # Add GCC/gfortran runtime directories used by Homebrew. These provide
    # @rpath/libquadmath*.dylib and @rpath/libgcc_s*.dylib.
    if (CMAKE_Fortran_COMPILER)
      get_filename_component (gfortran_bin_dir ${CMAKE_Fortran_COMPILER} DIRECTORY)
      get_filename_component (brew_prefix_from_fc ${gfortran_bin_dir}/.. REALPATH)
      if (EXISTS "${brew_prefix_from_fc}/lib/gcc/current")
        list (APPEND fixup_library_dirs "${brew_prefix_from_fc}/lib/gcc/current")
      endif ()
      if (EXISTS "${brew_prefix_from_fc}/opt/gcc/lib/gcc/current")
        list (APPEND fixup_library_dirs "${brew_prefix_from_fc}/opt/gcc/lib/gcc/current")
      endif ()
    endif ()

    # CMake discovers additional implicit linker search dirs for Fortran.
    # Include them so BundleUtilities can resolve compiler runtime dylibs.
    if (CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES)
      list (APPEND fixup_library_dirs ${CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES})
    endif ()

    # Also include explicit resolved paths when available.
    find_library (QUADMATH_LIBRARY NAMES quadmath libquadmath)
    if (QUADMATH_LIBRARY)
      get_filename_component (quadmath_lib_dir ${QUADMATH_LIBRARY} PATH)
      list (APPEND fixup_library_dirs ${quadmath_lib_dir})
    endif ()

    find_library (GCC_S_LIBRARY NAMES gcc_s.1.1 gcc_s libgcc_s)
    if (GCC_S_LIBRARY)
      get_filename_component (gcc_s_lib_dir ${GCC_S_LIBRARY} PATH)
      list (APPEND fixup_library_dirs ${gcc_s_lib_dir})
    endif ()

    # install required Qt plugins
    install (
      DIRECTORY
      ${QT_PLUGINS_DIR}/platforms
      ${QT_PLUGINS_DIR}/audio
      ${QT_PLUGINS_DIR}/accessible
      ${QT_PLUGINS_DIR}/imageformats
      ${QT_PLUGINS_DIR}/styles
      DESTINATION ${WSJT_PLUGIN_DESTINATION}
      CONFIGURATIONS Release MinSizeRel RelWithDebInfo
      #COMPONENT runtime
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}"
      PATTERN "*minimal*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*offscreen*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*quick*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*webgl*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*_debug${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}.dSYM" EXCLUDE
      )
    install (
      FILES
      ${QT_PLUGINS_DIR}/sqldrivers/libqsqlite${CMAKE_SHARED_LIBRARY_SUFFIX}
      DESTINATION ${WSJT_PLUGIN_DESTINATION}/sqldrivers
      CONFIGURATIONS Release MinSizeRel RelWithDebInfo
      #COMPONENT runtime
      )
    # install (
    # 	DIRECTORY
    # 	${QT_PLUGINS_DIR}/platforms
    # 	${QT_PLUGINS_DIR}/audio
    #         ${QT_PLUGINS_DIR}/accessible
    # 	DESTINATION ${WSJT_PLUGIN_DESTINATION}
    # 	CONFIGURATIONS Debug
    # 	#COMPONENT runtime
    # 	FILES_MATCHING PATTERN "*_debug${CMAKE_SHARED_LIBRARY_SUFFIX}"
    # 	PATTERN "*minimal*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	PATTERN "*offscreen*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	PATTERN "*quick*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	)

    # add plugins path for Mac Bundle
    install (CODE "
          get_filename_component (the_qt_conf \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${WSJT_QT_CONF_DESTINATION}/qt.conf\" REALPATH)
          file (APPEND \"\${the_qt_conf}\"
\"Plugins = PlugIns
\")
"
      #COMPONENT runtime
      )
  endif (APPLE)

  if (WIN32)
    # DLL directories
    get_filename_component (hamlib_lib_dir ${Hamlib_LIBRARY} PATH)
    list (APPEND fixup_library_dirs ${hamlib_lib_dir}/../bin)

    get_filename_component (usb_lib_dir ${Usb_LIBRARY} PATH)
    list (APPEND fixup_library_dirs ${usb_lib_dir})

    get_filename_component (portaudio_lib_dir ${Portaudio_LIBRARY} PATH)
    list (APPEND fixup_library_dirs ${portaudio_lib_dir}/../bin)

    get_filename_component (fftw_lib_dir ${FFTW3F_LIBRARY} PATH)
    list (APPEND fixup_library_dirs ${fftw_lib_dir})

    # install required Qt plugins
    install (
      DIRECTORY
      ${QT_PLUGINS_DIR}/platforms
      ${QT_PLUGINS_DIR}/styles
      ${QT_PLUGINS_DIR}/accessible
      ${QT_PLUGINS_DIR}/audio
      ${QT_PLUGINS_DIR}/imageformats
      DESTINATION ${WSJT_PLUGIN_DESTINATION}
      CONFIGURATIONS Release MinSizeRel RelWithDebInfo
      #COMPONENT runtime
      FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}"
      PATTERN "*minimal*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*offscreen*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*quick*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "*d${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
      )
    install (
      FILES
      ${QT_PLUGINS_DIR}/sqldrivers/qsqlite${CMAKE_SHARED_LIBRARY_SUFFIX}
      DESTINATION ${WSJT_PLUGIN_DESTINATION}/sqldrivers
      CONFIGURATIONS Release MinSizeRel RelWithDebInfo
      #COMPONENT runtime
      )
    # install (
    # 	DIRECTORY
    # 	${QT_PLUGINS_DIR}/platforms
    # 	${QT_PLUGINS_DIR}/accessible
    # 	${QT_PLUGINS_DIR}/audio
    # 	DESTINATION ${WSJT_PLUGIN_DESTINATION}
    # 	CONFIGURATIONS Debug
    # 	#COMPONENT runtime
    # 	FILES_MATCHING PATTERN "*d${CMAKE_SHARED_LIBRARY_SUFFIX}"
    # 	PATTERN "*minimal*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	PATTERN "*offscreen*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	PATTERN "*quick*${CMAKE_SHARED_LIBRARY_SUFFIX}" EXCLUDE
    # 	)

    # add paths for WIN32
    file (RELATIVE_PATH _plugins_path "${CMAKE_INSTALL_PREFIX}/${WSJT_QT_CONF_DESTINATION}" "${CMAKE_INSTALL_PREFIX}/${WSJT_PLUGIN_DESTINATION}")
    install (CODE "
        get_filename_component (the_qt_conf \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${WSJT_QT_CONF_DESTINATION}/qt.conf\" REALPATH)
        file (APPEND \"\${the_qt_conf}\"
\"Plugins = ${_plugins_path}
\")
"
      #COMPONENT runtime
      )

    set (gp_tool "objdump")	# we want MinGW tool - not MSVC (See GetPrerequisites.cmake)
  endif (WIN32)

  INSTALL (CODE "
        get_filename_component (the_path \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${WSJT_PLUGIN_DESTINATION}\" REALPATH)
        file (GLOB_RECURSE QTPLUGINS \"\${the_path}/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
        include (BundleUtilities)
        set (BU_CHMOD_BUNDLE_ITEMS ON)
        set (gp_tool ${gp_tool})
        # canonicalize path in install context
        get_filename_component (the_exe ${fixup_exe} REALPATH)
        fixup_bundle (\"\${the_exe}\" \"\${QTPLUGINS}\" \"${fixup_library_dirs}\")
        if (APPLE)
          # fixup_bundle may rewrite Mach-O load commands. Re-sign the bundle so
          # dyld on newer macOS versions does not reject modified pages.
          get_filename_component (bundle_macos_dir \"\${the_exe}\" DIRECTORY)
          get_filename_component (bundle_contents_dir \"\${bundle_macos_dir}\" DIRECTORY)
          get_filename_component (bundle_app \"\${bundle_contents_dir}\" DIRECTORY)
          find_program (CODESIGN_EXECUTABLE codesign)
          if (CODESIGN_EXECUTABLE)
            execute_process (
              COMMAND \"\${CODESIGN_EXECUTABLE}\" --force --deep --sign - --timestamp=none \"\${bundle_app}\"
              RESULT_VARIABLE _codesign_result
              OUTPUT_QUIET
              ERROR_VARIABLE _codesign_error
            )
            if (NOT _codesign_result EQUAL 0)
              message (FATAL_ERROR \"codesign failed for bundle '\${bundle_app}': \${_codesign_error}\")
            endif ()
          endif ()
        endif ()"
    #COMPONENT runtime
    )
endif (APPLE OR WIN32)

