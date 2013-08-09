
if (TOOLS)

    include(CheckFunctionExists)

    check_function_exists(getopt_long HAVE_GETOPT_LONG)
    if (NOT HAVE_GETOPT_LONG)
        message(SEND_ERROR "getopt_long required when compiling tools")
    endif (NOT HAVE_GETOPT_LONG)

endif (TOOLS)

