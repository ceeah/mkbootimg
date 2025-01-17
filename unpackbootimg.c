#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mincrypt/sha.h"
#include "mincrypt/sha256.h"
#include "bootimg.h"

typedef unsigned char byte;

int read_padding(FILE *f, unsigned itemsize, int pagesize)
{
    byte *buf = (byte *)malloc(sizeof(byte) * pagesize);
    unsigned pagemask = pagesize - 1;
    unsigned count;

    if((itemsize & pagemask) == 0) {
        free(buf);
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    if(fread(buf, count, 1, f)){};
    free(buf);
    return count;
}

void write_string_to_file(const char *file, const char *string)
{
    FILE *f = fopen(file, "w");
    fwrite(string, strlen(string), 1, f);
    fwrite("\n", 1, 1, f);
    fclose(f);
}

const char *detect_hash_type(boot_img_hdr_v2 *hdr)
{
    // sha1 is expected to have zeroes in id[20] and higher
    // offset by 4 to accomodate bootimg variants with BOOT_NAME_SIZE 20
    uint8_t id[SHA256_DIGEST_SIZE];
    memcpy(&id, hdr->id, sizeof(id));
    int i;
    for (i = SHA_DIGEST_SIZE + 4; i < SHA256_DIGEST_SIZE; ++i) {
        if (id[i]) {
            return "sha256";
        }
    }
    return "sha1";
}

int usage()
{
    printf("usage: unpackbootimg\n");
    printf("\t-i|--input boot.img\n");
    printf("\t[ -o|--output output_directory]\n");
    printf("\t[ -p|--pagesize <size-in-hexadecimal> ]\n");
    return 0;
}

/**
 * Unpack the boot.img with verion 3 header. 
 * f is expected to point to the start of the header
 */
int unpack_bootimg_v3(FILE *f, const char *directory, char *filename) {
    char tmp[PATH_MAX];
    boot_img_hdr_v3 header;

    if(fread(&header, sizeof(header), 1, f)){};

    printf("KERNEL_SIZE %u\n", header.kernel_size);
    printf("RAMDISK_SIZE %u\n", header.ramdisk_size);

    int a=0, b=0, c=0, y=0, m=0;

    if (header.os_version != 0) {
        int os_version,os_patch_level;
        os_version = header.os_version >> 11;
        os_patch_level = header.os_version&0x7ff;

        a = (os_version >> 14)&0x7f;
        b = (os_version >> 7)&0x7f;
        c = os_version&0x7f;

        y = (os_patch_level >> 4) + 2000;
        m = os_patch_level&0xf;

        if((a < 128) && (b < 128) && (c < 128) && (y >= 2000) && (y < 2128) && (m > 0) && (m <= 12)) {
            printf("BOARD_OS_VERSION %d.%d.%d\n", a, b, c);
            printf("BOARD_OS_PATCH_LEVEL %d-%02d\n", y, m);
        } else {
            header.os_version = 0;
        }
    }
    
    if (header.os_version != 0) {
        //printf("os_version...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_version");
        char osvertmp[200];
        sprintf(osvertmp, "%d.%d.%d", a, b, c);
        write_string_to_file(tmp, osvertmp);

        //printf("os_patch_level...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_patch_level");
        char oslvltmp[200];
        sprintf(oslvltmp, "%d-%02d", y, m);
        write_string_to_file(tmp, oslvltmp);
    }

    //printf("header_version...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-header_version");
    char hdrvertmp[200];
    sprintf(hdrvertmp, "%d\n", header.header_version);
    write_string_to_file(tmp, hdrvertmp);

    //printf("cmdline...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-cmdline");
    char cmdlinetmp[BOOT_ARGS_SIZE+BOOT_EXTRA_ARGS_SIZE+1];
    sprintf(cmdlinetmp, "%.*s", BOOT_ARGS_SIZE, header.cmdline);
    cmdlinetmp[BOOT_ARGS_SIZE+BOOT_EXTRA_ARGS_SIZE]='\0';
    write_string_to_file(tmp, cmdlinetmp);

    //printf("total read: %d\n", header.kernel_size);
    read_padding(f, header.header_size, 4096);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-zImage");
    FILE *k = fopen(tmp, "wb");
    byte *kernel = (byte *)malloc(header.kernel_size);
    //printf("Reading kernel...\n");
    fseek(f, header.header_size - sizeof(header), SEEK_CUR);
    if(fread(kernel, header.kernel_size, 1, f)){};
    //total_read += header.kernel_size;
    fwrite(kernel, header.kernel_size, 1, k);
    fclose(k);

    //printf("total read: %d\n", header.kernel_size);
    read_padding(f, header.kernel_size, 4096);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk.gz");
    FILE *r = fopen(tmp, "wb");
    byte *ramdisk = (byte *)malloc(header.ramdisk_size);
    //printf("Reading ramdisk...\n");
    if(fread(ramdisk, header.ramdisk_size, 1, f)){};
    //total_read += header.ramdisk_size;
    fwrite(ramdisk, header.ramdisk_size, 1, r);
    fclose(r);

    fclose(f);

    return 0;
}


int main(int argc, char **argv)
{
    char tmp[PATH_MAX];
    char *directory = "./";
    char *filename = NULL;
    int pagesize = 0;
    int base = 0;

    int seeklimit = 65536; // arbitrary byte limit to search in input file for ANDROID! magic
    int hdr_ver_max = 4; // arbitrary maximum header version value; when greater assume the field is appended dtb size

    argc--;
    argv++;
    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            filename = val;
        } else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            directory = val;
        } else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            pagesize = strtoul(val, 0, 16);
        } else {
            return usage();
        }
    }

    if (filename == NULL) {
        return usage();
    }

    struct stat st;
    if (stat(directory, &st) == (-1)) {
        printf("Could not stat %s: %s\n", directory, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        printf("%s is not a directory\n", directory);
        return 1;
    }

    int total_read = 0;
    FILE *f = fopen(filename, "rb");
    boot_img_hdr_v2 header;

    if (!f) {
        printf("Could not open input file: %s\n", strerror(errno));
        return (1);
    }

    //printf("Reading header...\n");
    int i;
    for (i = 0; i <= seeklimit; i++) {
        fseek(f, i, SEEK_SET);
        if(fread(tmp, BOOT_MAGIC_SIZE, 1, f)){};
        if (memcmp(tmp, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0) {
            break;
        }
    }
    total_read = i;
    if (i > seeklimit) {
        printf("Android boot magic not found.\n");
        return 1;
    }
    fseek(f, i, SEEK_SET);
    if (i > 0) {
        printf("Android magic found at: %d\n", i);
    }

    if(fread(&header, sizeof(header), 1, f)){};

    printf("HEADER_VERSION %u\n", header.header_version);

    if (header.header_version == 3) {
        fseek(f, i, SEEK_SET);
        return unpack_bootimg_v3(f, directory, filename);
    }

    base = header.kernel_addr - 0x00008000;
    printf("BOARD_KERNEL_CMDLINE %.*s%.*s\n", BOOT_ARGS_SIZE, header.cmdline, BOOT_EXTRA_ARGS_SIZE, header.extra_cmdline);
    printf("BOARD_KERNEL_BASE 0x%08x\n", base);
    printf("BOARD_NAME %s\n", header.name);
    printf("BOARD_PAGE_SIZE %d\n", header.page_size);
    printf("BOARD_HASH_TYPE %s\n", detect_hash_type(&header));
    printf("BOARD_KERNEL_OFFSET 0x%08x\n", header.kernel_addr - base);
    printf("BOARD_RAMDISK_OFFSET 0x%08x\n", header.ramdisk_addr - base);
    printf("BOARD_SECOND_OFFSET 0x%08x\n", header.second_addr - base);
    printf("BOARD_TAGS_OFFSET 0x%08x\n", header.tags_addr - base);

    int a=0, b=0, c=0, y=0, m=0;
    if (header.os_version != 0) {
        int os_version,os_patch_level;
        os_version = header.os_version >> 11;
        os_patch_level = header.os_version&0x7ff;

        a = (os_version >> 14)&0x7f;
        b = (os_version >> 7)&0x7f;
        c = os_version&0x7f;

        y = (os_patch_level >> 4) + 2000;
        m = os_patch_level&0xf;

        if((a < 128) && (b < 128) && (c < 128) && (y >= 2000) && (y < 2128) && (m > 0) && (m <= 12)) {
            printf("BOARD_OS_VERSION %d.%d.%d\n", a, b, c);
            printf("BOARD_OS_PATCH_LEVEL %d-%02d\n", y, m);
        } else {
            header.os_version = 0;
        }
    }

    if (header.dt_size > hdr_ver_max) {
        printf("BOARD_DT_SIZE %d\n", header.dt_size);
    } else {
        printf("BOARD_HEADER_VERSION %d\n", header.header_version);
    }
    if (header.header_version <= hdr_ver_max) {
        if (header.header_version > 0) {
            if (header.recovery_dtbo_size != 0) {
                printf("BOARD_RECOVERY_DTBO_SIZE %d\n", header.recovery_dtbo_size);
                printf("BOARD_RECOVERY_DTBO_OFFSET %"PRId64"\n", header.recovery_dtbo_offset);
            }
            printf("BOARD_HEADER_SIZE %d\n", header.header_size);
        } else {
            header.recovery_dtbo_size = 0;
        }
        if (header.header_version > 1) {
            if (header.dtb_size != 0) {
                printf("BOARD_DTB_SIZE %d\n", header.dtb_size);
                printf("BOARD_DTB_OFFSET 0x%08"PRIx64"\n", header.dtb_addr - base);
            }
        } else {
            header.dtb_size = 0;
        }
    }

    if (pagesize == 0) {
        pagesize = header.page_size;
    }

    //printf("cmdline...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-cmdline");
    char cmdlinetmp[BOOT_ARGS_SIZE+BOOT_EXTRA_ARGS_SIZE+1];
    sprintf(cmdlinetmp, "%.*s%.*s", BOOT_ARGS_SIZE, header.cmdline, BOOT_EXTRA_ARGS_SIZE, header.extra_cmdline);
    cmdlinetmp[BOOT_ARGS_SIZE+BOOT_EXTRA_ARGS_SIZE]='\0';
    write_string_to_file(tmp, cmdlinetmp);

    //printf("board...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-board");
    write_string_to_file(tmp, (char *)header.name);

    //printf("base...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-base");
    char basetmp[200];
    sprintf(basetmp, "0x%08x", base);
    write_string_to_file(tmp, basetmp);

    //printf("pagesize...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-pagesize");
    char pagesizetmp[200];
    sprintf(pagesizetmp, "%d", header.page_size);
    write_string_to_file(tmp, pagesizetmp);

    //printf("kernel_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-kernel_offset");
    char kernelofftmp[200];
    sprintf(kernelofftmp, "0x%08x", header.kernel_addr - base);
    write_string_to_file(tmp, kernelofftmp);

    //printf("ramdisk_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk_offset");
    char ramdiskofftmp[200];
    sprintf(ramdiskofftmp, "0x%08x", header.ramdisk_addr - base);
    write_string_to_file(tmp, ramdiskofftmp);

    //printf("second_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-second_offset");
    char secondofftmp[200];
    sprintf(secondofftmp, "0x%08x", header.second_addr - base);
    write_string_to_file(tmp, secondofftmp);

    //printf("tags_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-tags_offset");
    char tagsofftmp[200];
    sprintf(tagsofftmp, "0x%08x", header.tags_addr - base);
    write_string_to_file(tmp, tagsofftmp);

    if (header.os_version != 0) {
        //printf("os_version...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_version");
        char osvertmp[200];
        sprintf(osvertmp, "%d.%d.%d", a, b, c);
        write_string_to_file(tmp, osvertmp);

        //printf("os_patch_level...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_patch_level");
        char oslvltmp[200];
        sprintf(oslvltmp, "%d-%02d", y, m);
        write_string_to_file(tmp, oslvltmp);
    }

    if (header.header_version <= hdr_ver_max) {
        //printf("header_version...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-header_version");
        char hdrvertmp[200];
        sprintf(hdrvertmp, "%d\n", header.header_version);
        write_string_to_file(tmp, hdrvertmp);

        if (header.header_version > 1) {
            //printf("dtb_offset...\n");
            sprintf(tmp, "%s/%s", directory, basename(filename));
            strcat(tmp, "-dtb_offset");
            char dtbofftmp[200];
            sprintf(dtbofftmp, "0x%08"PRIx64, header.dtb_addr - base);
            write_string_to_file(tmp, dtbofftmp);
        }
    }

    //printf("hashtype...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-hashtype");
    const char *hash_type = detect_hash_type(&header);
    write_string_to_file(tmp, hash_type);

    total_read += sizeof(header);
    //printf("total read: %d\n", total_read);
    total_read += read_padding(f, sizeof(header), pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-zImage");
    FILE *k = fopen(tmp, "wb");
    byte *kernel = (byte *)malloc(header.kernel_size);
    //printf("Reading kernel...\n");
    if(fread(kernel, header.kernel_size, 1, f)){};
    total_read += header.kernel_size;
    fwrite(kernel, header.kernel_size, 1, k);
    fclose(k);

    //printf("total read: %d\n", header.kernel_size);
    total_read += read_padding(f, header.kernel_size, pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk.gz");
    FILE *r = fopen(tmp, "wb");
    byte *ramdisk = (byte *)malloc(header.ramdisk_size);
    //printf("Reading ramdisk...\n");
    if(fread(ramdisk, header.ramdisk_size, 1, f)){};
    total_read += header.ramdisk_size;
    fwrite(ramdisk, header.ramdisk_size, 1, r);
    fclose(r);

    //printf("total read: %d\n", header.ramdisk_size);
    total_read += read_padding(f, header.ramdisk_size, pagesize);

    if (header.second_size != 0) {
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-second");
        FILE *s = fopen(tmp, "wb");
        byte *second = (byte *)malloc(header.second_size);
        //printf("Reading second...\n");
        if(fread(second, header.second_size, 1, f)){};
        total_read += header.second_size;
        fwrite(second, header.second_size, 1, s);
        fclose(s);
    }

    //printf("total read: %d\n", header.second_size);
    total_read += read_padding(f, header.second_size, pagesize);

    if (header.dt_size > hdr_ver_max) {
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-dt");
        FILE *d = fopen(tmp, "wb");
        byte *dt = (byte *)malloc(header.dt_size);
        //printf("Reading dt...\n");
        if(fread(dt, header.dt_size, 1, f)){};
        total_read += header.dt_size;
        fwrite(dt, header.dt_size, 1, d);
        fclose(d);
    } else {
        if (header.recovery_dtbo_size != 0) {
            sprintf(tmp, "%s/%s", directory, basename(filename));
            strcat(tmp, "-recovery_dtbo");
            FILE *o = fopen(tmp, "wb");
            byte *dtbo = (byte *)malloc(header.recovery_dtbo_size);
            //printf("Reading recovery_dtbo...\n");
            if(fread(dtbo, header.recovery_dtbo_size, 1, f)){};
            total_read += header.recovery_dtbo_size;
            fwrite(dtbo, header.recovery_dtbo_size, 1, o);
            fclose(o);
        }

        //printf("total read: %d\n", header.recovery_dtbo_size);
        total_read += read_padding(f, header.recovery_dtbo_size, pagesize);

        if (header.dtb_size != 0) {
            sprintf(tmp, "%s/%s", directory, basename(filename));
            strcat(tmp, "-dtb");
            FILE *b = fopen(tmp, "wb");
            byte *dtb = (byte *)malloc(header.dtb_size);
            //printf("Reading dtb...\n");
            if(fread(dtb, header.dtb_size, 1, f)){};
            total_read += header.dtb_size;
            fwrite(dtb, header.dtb_size, 1, b);
            fclose(b);
        }
    }

    fclose(f);

    //printf("Total Read: %d\n", total_read);
    return 0;
}

