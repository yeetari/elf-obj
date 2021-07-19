#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Symbol {
    const char *name;
    Elf64_Xword size;
    uint8_t type;
};

static int compare_symbols(const void *a, const void *b) {
    const struct Symbol *lhs = (struct Symbol *)a;
    const struct Symbol *rhs = (struct Symbol *)b;
    if (lhs->size < rhs->size) {
        return 1;
    }
    if (lhs->size > rhs->size) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <binary>\n", argv[0]);
        return 0;
    }
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    uint8_t magic[SELFMAG];
    if (fread(magic, sizeof(uint8_t), SELFMAG, file) != SELFMAG || memcmp(magic, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        fclose(file);
        return 1;
    }

    if (fseek(file, 0, SEEK_SET) < 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }

    Elf64_Ehdr header;
    if (fread(&header, sizeof(Elf64_Ehdr), 1, file) != 1) {
        fprintf(stderr, "Failed to read ELF64 header\n");
        fclose(file);
        return 1;
    }

    if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        fclose(file);
        return 1;
    }

    for (uint16_t i = 0; i < header.e_shnum; i++) {
        if (fseek(file, header.e_shoff + header.e_shentsize * i, SEEK_SET) < 0) {
            perror("fseek");
            fclose(file);
            return 1;
        }
        Elf64_Shdr shdr;
        if (fread(&shdr, sizeof(Elf64_Shdr), 1, file) != 1) {
            fprintf(stderr, "Failed to read section header\n");
            fclose(file);
            return 1;
        }
        if (shdr.sh_type != SHT_SYMTAB && shdr.sh_type != SHT_DYNSYM) {
            continue;
        }
        const Elf64_Word str_tab_idx = shdr.sh_link;
        if (fseek(file, header.e_shoff + header.e_shentsize * str_tab_idx, SEEK_SET) < 0) {
            perror("fseek");
            fclose(file);
            return 1;
        }
        Elf64_Shdr str_tab_shdr;
        if (fread(&str_tab_shdr, sizeof(Elf64_Shdr), 1, file) != 1) {
            fprintf(stderr, "Failed to read string table section header\n");
            fclose(file);
            return 1;
        }
        if (fseek(file, str_tab_shdr.sh_offset, SEEK_SET) < 0) {
            perror("fseek");
            fclose(file);
            return 1;
        }
        char *str_tab = malloc(str_tab_shdr.sh_size);
        if (fread(str_tab, sizeof(char), str_tab_shdr.sh_size, file) != str_tab_shdr.sh_size) {
            fprintf(stderr, "Failed to read string table\n");
            free(str_tab);
            fclose(file);
            return 1;
        }

        if (fseek(file, shdr.sh_offset, SEEK_SET) < 0) {
            perror("fseek");
            free(str_tab);
            fclose(file);
            return 1;
        }
        if (shdr.sh_entsize < sizeof(Elf64_Sym) || shdr.sh_size % shdr.sh_entsize != 0) {
            fprintf(stderr, "Invalid symbol table\n");
            free(str_tab);
            fclose(file);
            return 1;
        }

        const Elf64_Xword symbol_count = shdr.sh_size / shdr.sh_entsize;
        struct Symbol *symbols = calloc(symbol_count, sizeof(struct Symbol));
        for (Elf64_Xword j = 0; j < symbol_count; j++) {
            Elf64_Sym sym;
            if (fread(&sym, sizeof(Elf64_Sym), 1, file) != 1) {
                fprintf(stderr, "Failed to read symbol table entry\n");
                free(symbols);
                free(str_tab);
                fclose(file);
                return 1;
            }

            struct Symbol *symbol = &symbols[j];
            symbol->name = &str_tab[sym.st_name];
            symbol->size = sym.st_size;
            symbol->type = sym.st_info & 0xfu;

            if (shdr.sh_entsize > sizeof(Elf64_Sym)) {
                if (fseek(file, shdr.sh_entsize - sizeof(Elf64_Sym), SEEK_CUR) < 0) {
                    perror("fseek");
                    free(symbols);
                    free(str_tab);
                    fclose(file);
                    return 1;
                }
            }
        }

        qsort(symbols, symbol_count, sizeof(struct Symbol), &compare_symbols);
        for (Elf64_Xword j = 0; j < symbol_count; j++) {
            if (symbols[j].type != STT_OBJECT) {
                continue;
            }
            printf("%s - %lu bytes\n", symbols[j].name, symbols[j].size);
        }
        free(symbols);
        free(str_tab);
    }
    fclose(file);
}
