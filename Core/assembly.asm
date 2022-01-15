PerformByteTransaction:
080032cc:   ldr     r3, [pc, #20]   // (0x80032e4 <PerformByteTransaction+24>)
080032ce:   ldr     r3, [r3, #0]    // Loading _spiInstance address in r3
080032d0:   str     r0, [r3, #12]   // Storing r0 (byte parameter) in r3 + 12 [SPI Data Register]
080032d2:   ldr     r2, [r3, #8]    // Loading r3 + 8 (SPI Status Register) value into r2
080032d4:   and.w   r2, r2, #3      // We need to check Tx event and Rx event
080032d8:   cmp     r2, #3
080032da:   bne.n   <PerformByteTransaction+6> // Loop waiting
080032dc:   ldr     r0, [r3, #12]   // Tx and Rx completed; We need to read Data Register again
080032de:   uxtb    r0, r0 	    // Zero extend byte
080032e0:   bx      lr		    // Return to caller
