// Ends a game module's .eh_frame, as crtend.o ends a program's: linked last into vcs_game.so,
// so __register_frame knows where the module's unwind tables stop.
    .section .eh_frame,"a",%progbits
    .4byte 0
