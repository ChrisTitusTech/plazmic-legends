cmake_minimum_required(VERSION 3.28)

project(
    PlazmicLegends
    VERSION 0.1.0
    DESCRIPTION "Native Linux research tools for the EverQuest Legends overlay"
    LANGUAGES NONE
)

include(CTest)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
find_program(RUFF_EXECUTABLE NAMES ruff REQUIRED)
find_program(MARKDOWNLINT_EXECUTABLE NAMES markdownlint-cli2 REQUIRED)

set(PLAZMIC_MARKDOWN_FILES
    AGENTS.md
    README.md
    ROADMAP.md
    SPEC.md
    TASKS.md
    "docs/**/*.md"
)

add_custom_target(
    check
    COMMAND
        "${MARKDOWNLINT_EXECUTABLE}"
        ${PLAZMIC_MARKDOWN_FILES}
    COMMAND
        "${RUFF_EXECUTABLE}"
        check
        tools/inspect_eqgame.py
        tests/test_inspect_eqgame.py
    COMMAND
        "${RUFF_EXECUTABLE}"
        format
        --check
        tools/inspect_eqgame.py
        tests/test_inspect_eqgame.py
    COMMAND
        "${Python3_EXECUTABLE}"
        -m
        py_compile
        tools/inspect_eqgame.py
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
    COMMENT "Running the Phase 0 repository gate"
    VERBATIM
)

if(BUILD_TESTING)
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
