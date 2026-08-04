cmake_minimum_required(VERSION 3.28)

project(
    PlazmicLegends
    VERSION 0.2.0
    DESCRIPTION "Native Linux EverQuest Legends companion application"
    LANGUAGES CXX
)

include(CTest)
include(CheckPIESupported)
include(GNUInstallDirs)
check_pie_supported()

option(
    PLAZMIC_ENABLE_REPOSITORY_CHECKS
    "Require and expose the repository lint and source-validation tools"
    ON
)

if(BUILD_TESTING OR PLAZMIC_ENABLE_REPOSITORY_CHECKS)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
endif()
find_package(X11 REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core DBus Gui Widgets)

if(PLAZMIC_ENABLE_REPOSITORY_CHECKS)
    find_program(RUFF_EXECUTABLE NAMES ruff REQUIRED)
    find_program(MARKDOWNLINT_EXECUTABLE NAMES markdownlint-cli2 REQUIRED)
endif()

add_library(
    plazmic_core
    STATIC
    src/common/client_file_monitor.cpp
    src/common/sha256.cpp
    src/game/character_reader.cpp
    src/game/combat_log_parser.cpp
    src/game/client_profile.cpp
    src/game/game_state_reader.cpp
    src/game/spawn_reader.cpp
    src/integration/process_discovery.cpp
    src/integration/process_reader.cpp
    src/launcher/client_selection.cpp
    src/launcher/client_status.cpp
    src/launcher/player_lifecycle.cpp
    src/launcher/privacy_log.cpp
    src/map/map_parser.cpp
    src/map/map_view_model.cpp
    src/model/status_snapshot.cpp
)

target_compile_features(plazmic_core PUBLIC cxx_std_20)
set_target_properties(
    plazmic_core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)
target_compile_options(
    plazmic_core
    PRIVATE
        -fstack-protector-strong
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Werror
)
target_include_directories(
    plazmic_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_executable(
    plazmic-legends
    src/ui/main.cpp
    src/ui/map_canvas.cpp
    src/ui/main_window.cpp
    src/ui/spawn_table_model.cpp
    src/ui/system_theme.cpp
    src/ui/ui_file_installer.cpp
    src/ui/ui_settings.cpp
    src/ui/x11_window_class.cpp
)
target_compile_features(plazmic-legends PRIVATE cxx_std_20)
target_compile_definitions(
    plazmic-legends
    PRIVATE
        PLAZMIC_VERSION="${PROJECT_VERSION}"
)
target_compile_options(
    plazmic-legends
    PRIVATE
        -fstack-protector-strong
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Werror
)
set_target_properties(
    plazmic-legends
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)
target_link_options(
    plazmic-legends
    PRIVATE
        -Wl,-z,now
        -Wl,-z,relro
        -Wl,-z,noexecstack
)
target_link_libraries(
    plazmic-legends
    PRIVATE
        plazmic_core
        Qt6::Concurrent
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Widgets
        X11::X11
)
qt_add_resources(
    plazmic-legends
    "plazmic_resources"
    PREFIX "/icons"
    BASE "packaging"
    FILES packaging/plazmic-legends.png
)

install(
    TARGETS plazmic-legends
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
    FILES packaging/plazmic-legends.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
install(
    FILES packaging/plazmic-legends.png
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
)
install(
    FILES packaging/io.github.ChristitusTech.PlazmicLegends.metainfo.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)
install(
    FILES LICENSE
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/plazmic-legends"
)
install(
    FILES
        README.md
        development.md
        docs/package-operations.md
        docs/phase5-product-boundary.md
        packaging/THIRD-PARTY-NOTICES.md
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/plazmic-legends"
)

set(PLAZMIC_MARKDOWN_FILES
    ".github/**/*.md"
    AGENTS.md
    CODE_OF_CONDUCT.md
    CONTRIBUTING.md
    development.md
    README.md
    ROADMAP.md
    SPEC.md
    TASKS.md
    "docs/**/*.md"
)

if(PLAZMIC_ENABLE_REPOSITORY_CHECKS)
    add_custom_target(
        check
        COMMAND
            "${MARKDOWNLINT_EXECUTABLE}"
            ${PLAZMIC_MARKDOWN_FILES}
        COMMAND
            "${RUFF_EXECUTABLE}"
            check
            tools/create_cohesive_ui_layout.py
            tools/inspect_eqgame.py
            tests/test_create_cohesive_ui_layout.py
            tests/test_export_private_ui_bundle.py
            tests/test_inspect_eqgame.py
        COMMAND
            "${RUFF_EXECUTABLE}"
            format
            --check
            tools/create_cohesive_ui_layout.py
            tools/inspect_eqgame.py
            tests/test_create_cohesive_ui_layout.py
            tests/test_export_private_ui_bundle.py
            tests/test_inspect_eqgame.py
        COMMAND
            "${Python3_EXECUTABLE}"
            -m
            py_compile
            tools/create_cohesive_ui_layout.py
            tools/inspect_eqgame.py
            tests/test_create_cohesive_ui_layout.py
            tests/test_export_private_ui_bundle.py
            tests/test_inspect_eqgame.py
        COMMAND
            "${Python3_EXECUTABLE}"
            -m
            unittest
            discover
            -s
            tests
            -p
            "test_*.py"
            -v
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running the repository gate"
        VERBATIM
    )
else()
    add_custom_target(check)
endif()
add_dependencies(check plazmic-legends)

if(BUILD_TESTING)
    add_executable(
        process_discovery_test
        tests/test_process_discovery.cpp
    )
    target_compile_features(process_discovery_test PRIVATE cxx_std_20)
    target_compile_options(
        process_discovery_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(process_discovery_test PRIVATE plazmic_core)
    add_dependencies(check process_discovery_test)

    add_test(NAME process_discovery.unit COMMAND process_discovery_test)

    add_executable(character_reader_test tests/test_character_reader.cpp)
    target_compile_features(character_reader_test PRIVATE cxx_std_20)
    target_compile_options(
        character_reader_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(character_reader_test PRIVATE plazmic_core)
    add_dependencies(check character_reader_test)
    add_test(NAME character_reader.unit COMMAND character_reader_test)

    add_executable(client_selection_test tests/test_client_selection.cpp)
    target_compile_features(client_selection_test PRIVATE cxx_std_20)
    target_compile_options(
        client_selection_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(client_selection_test PRIVATE plazmic_core)
    add_dependencies(check client_selection_test)
    add_test(NAME client_selection.unit COMMAND client_selection_test)

    add_executable(client_status_test tests/test_client_status.cpp)
    target_compile_features(client_status_test PRIVATE cxx_std_20)
    target_compile_options(
        client_status_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(client_status_test PRIVATE plazmic_core)
    add_dependencies(check client_status_test)
    add_test(NAME client_status.unit COMMAND client_status_test)

    add_executable(privacy_log_test tests/test_privacy_log.cpp)
    target_compile_features(privacy_log_test PRIVATE cxx_std_20)
    target_compile_options(
        privacy_log_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(privacy_log_test PRIVATE plazmic_core)
    add_dependencies(check privacy_log_test)
    add_test(NAME privacy_log.unit COMMAND privacy_log_test)

    add_executable(map_parser_test tests/test_map_parser.cpp)
    target_compile_features(map_parser_test PRIVATE cxx_std_20)
    target_compile_options(
        map_parser_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(map_parser_test PRIVATE plazmic_core)
    add_dependencies(check map_parser_test)
    add_test(NAME map_parser.unit COMMAND map_parser_test)

    add_executable(map_view_model_test tests/test_map_view_model.cpp)
    target_compile_features(map_view_model_test PRIVATE cxx_std_20)
    target_compile_options(
        map_view_model_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(map_view_model_test PRIVATE plazmic_core)
    add_dependencies(check map_view_model_test)
    add_test(NAME map_view_model.unit COMMAND map_view_model_test)

    add_executable(
        game_state_reader_test
        tests/test_game_state_reader.cpp
    )
    target_compile_features(game_state_reader_test PRIVATE cxx_std_20)
    target_compile_options(
        game_state_reader_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(game_state_reader_test PRIVATE plazmic_core)
    add_dependencies(check game_state_reader_test)
    add_test(NAME game_state_reader.unit COMMAND game_state_reader_test)

    add_executable(
        combat_log_parser_test
        tests/test_combat_log_parser.cpp
    )
    target_compile_features(combat_log_parser_test PRIVATE cxx_std_20)
    target_compile_options(
        combat_log_parser_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(combat_log_parser_test PRIVATE plazmic_core)
    add_dependencies(check combat_log_parser_test)
    add_test(NAME combat_log_parser.unit COMMAND combat_log_parser_test)

    add_executable(
        player_lifecycle_test
        tests/test_player_lifecycle.cpp
    )
    target_compile_features(player_lifecycle_test PRIVATE cxx_std_20)
    target_compile_options(
        player_lifecycle_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_link_libraries(player_lifecycle_test PRIVATE plazmic_core)
    add_dependencies(check player_lifecycle_test)
    add_test(NAME player_lifecycle.unit COMMAND player_lifecycle_test)

    add_executable(
        ui_settings_test
        tests/test_ui_settings.cpp
        src/ui/ui_settings.cpp
    )
    target_compile_features(ui_settings_test PRIVATE cxx_std_20)
    target_compile_options(
        ui_settings_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_include_directories(
        ui_settings_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(ui_settings_test PRIVATE Qt6::Core)
    add_dependencies(check ui_settings_test)
    add_test(NAME ui_settings.unit COMMAND ui_settings_test)

    add_executable(
        ui_file_installer_test
        tests/test_ui_file_installer.cpp
        src/ui/ui_file_installer.cpp
    )
    target_compile_features(ui_file_installer_test PRIVATE cxx_std_20)
    target_compile_options(
        ui_file_installer_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
    target_include_directories(
        ui_file_installer_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(
        ui_file_installer_test
        PRIVATE
            Qt6::Core
    )
    add_dependencies(check ui_file_installer_test)
    add_test(NAME ui_file_installer.unit COMMAND ui_file_installer_test)

    find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
    if(XVFB_RUN_EXECUTABLE)
        add_executable(
            main_window_test
            tests/test_main_window.cpp
            src/ui/map_canvas.cpp
            src/ui/main_window.cpp
            src/ui/spawn_table_model.cpp
            src/ui/ui_file_installer.cpp
            src/ui/ui_settings.cpp
            src/ui/x11_window_class.cpp
        )
        target_compile_features(main_window_test PRIVATE cxx_std_20)
        target_compile_options(
            main_window_test
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
        target_include_directories(
            main_window_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            main_window_test
            PRIVATE
                plazmic_core
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
                X11::X11
        )
        add_dependencies(check main_window_test)
        add_test(
            NAME main_window.x11
            COMMAND
                "${XVFB_RUN_EXECUTABLE}"
                -a
                "$<TARGET_FILE:main_window_test>"
        )
        set_tests_properties(
            main_window.x11
            PROPERTIES
                SKIP_RETURN_CODE 77
                TIMEOUT 15
        )

        add_executable(
            phase3_performance_test
            tests/test_phase3_performance.cpp
            src/ui/map_canvas.cpp
        )
        target_compile_features(
            phase3_performance_test
            PRIVATE cxx_std_20
        )
        target_compile_options(
            phase3_performance_test
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
        target_include_directories(
            phase3_performance_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            phase3_performance_test
            PRIVATE
                plazmic_core
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
        )
        add_dependencies(check phase3_performance_test)
        add_test(
            NAME phase3_performance.x11
            COMMAND
                "${XVFB_RUN_EXECUTABLE}"
                -a
                "$<TARGET_FILE:phase3_performance_test>"
        )
        set_tests_properties(
            phase3_performance.x11
            PROPERTIES
                TIMEOUT 30
        )

        add_executable(
            phase4_performance_test
            tests/test_phase4_performance.cpp
            src/ui/map_canvas.cpp
            src/ui/spawn_table_model.cpp
        )
        target_compile_features(
            phase4_performance_test
            PRIVATE cxx_std_20
        )
        target_compile_options(
            phase4_performance_test
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
        target_include_directories(
            phase4_performance_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            phase4_performance_test
            PRIVATE
                plazmic_core
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
        )
        add_dependencies(check phase4_performance_test)
        add_test(
            NAME phase4_performance.x11
            COMMAND
                "${XVFB_RUN_EXECUTABLE}"
                -a
                "$<TARGET_FILE:phase4_performance_test>"
        )
        set_tests_properties(
            phase4_performance.x11
            PROPERTIES
                TIMEOUT 30
        )

        add_executable(
            system_theme_test
            tests/test_system_theme.cpp
            src/ui/system_theme.cpp
        )
        target_compile_features(system_theme_test PRIVATE cxx_std_20)
        target_compile_options(
            system_theme_test
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
        target_include_directories(
            system_theme_test
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(
            system_theme_test
            PRIVATE
                Qt6::Core
                Qt6::DBus
                Qt6::Gui
                Qt6::Widgets
        )
        add_dependencies(check system_theme_test)
        add_test(
            NAME system_theme.x11
            COMMAND
                "${XVFB_RUN_EXECUTABLE}"
                -a
                "$<TARGET_FILE:system_theme_test>"
        )
        set_tests_properties(
            system_theme.x11
            PROPERTIES
                TIMEOUT 15
        )

    endif()

    add_test(
        NAME inspect_eqgame.unit
        COMMAND
            "${Python3_EXECUTABLE}"
            -m
            unittest
            discover
            -s
            tests
            -p
            "test_*.py"
            -v
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

set(
    EQ_LEGENDS_DIR
    ""
    CACHE PATH
    "Optional path to the local EverQuest Legends installation"
)

if(EQ_LEGENDS_DIR)
    add_custom_target(
        check-client
        COMMAND
            "${Python3_EXECUTABLE}"
            tools/inspect_eqgame.py
            "${EQ_LEGENDS_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Inspecting the configured local Legends client"
        VERBATIM
    )
endif()
