/*
I want to make a program that fills a txt file with the following content:
    1000 lines of virtual addresses:
        • Random (provided addresses.txt)
        • Sequential (e.g., 0 → 65535)
        • Looping / locality-heavy (small set repeated)
        • Stride-based (e.g., every k-th address)
*/
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    FILE *file = fopen("addressesLoop.txt", "w");
    if (!file) {
        fprintf(stderr, "Error opening file for writing\n");
        return EXIT_FAILURE;
    }
    
    // // Generate 1000 lines of virtual addresses
    // for (int i = 0; i < 1000; i++) {
    //     int address = rand() % 65536; // Random address in the range 0 to 65535
    //     fprintf(file, "%d\n", address);
    // }

    // //Generate 1000 lines of sequential virtual addresses
    // for (int i = 0; i < 65535; i++) {
    //     fprintf(file, "%d\n", i); // Sequential addresses from 0 to 65535
    // }

    // // Looping / locality-heavy (small set repeated)
    for(int i = 0; i < 1000; i++) {
        int address = (i % 10) * 256; // Looping through a small set of addresses
        fprintf(file, "%d\n", address);
    }

    // Stride-based (every k-th address)
    // int k = 256; // Stride of 256
    // for (int i = 0; i < 1000; i++) {
    //     int address = (i * k) % 65536; // Stride-based addresses
    //     fprintf(file, "%d\n", address);
    // }
    fclose(file);
    return EXIT_SUCCESS;
}