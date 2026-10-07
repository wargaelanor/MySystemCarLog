set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_VWEGOLF)
  # DEV: upstream dc86d82da lists src/vehicle_vwegolf_climate.cpp which was never committed,
  # and omits the real src/vehicle_vwegolf_bat_ctrl.cpp - fixed here (DEVIATIONS.md).
  list(APPEND srcs "src/vehicle_vwegolf.cpp" "src/vehicle_vwegolf_bat_ctrl.cpp"
                   "src/bap/bap.cpp" "src/egolf/battery_control.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
