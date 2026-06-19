# m6502recomp - the static recompiler CLI (decode / analyze / emit).
add_executable(m6502recomp
    main.c
    decode.c
    analyze.c
    emit.c
    a2regs.c
)
set_target_properties(m6502recomp PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(m6502recomp PRIVATE /W3)
else()
    target_compile_options(m6502recomp PRIVATE -Wall -Wextra)
endif()
