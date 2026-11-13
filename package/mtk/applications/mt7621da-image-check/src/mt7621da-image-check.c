/* SPDX-License-Identifier: GPL-2.0-only */
/* Product layout check. fwtool must verify the complete image CRC/metadata first. */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libfdt.h>
#include <openssl/sha.h>

#define FW_SIZE 0x7b0000u
#define FW_OFFSET 0x50000u
#define ERASE_SIZE 0x10000u

static int bounds_fdt(const void *p, size_t size)
{
 uint32_t total, off, len;
 if (size < sizeof(struct fdt_header) || fdt_check_header(p)) return 0;
 total = fdt_totalsize(p);
 if (total < sizeof(struct fdt_header) || total > size) return 0;
 off = fdt_off_dt_struct(p); len = fdt_size_dt_struct(p);
 if (off > total || len > total - off) return 0;
 off = fdt_off_dt_strings(p); len = fdt_size_dt_strings(p);
 if (off > total || len > total - off) return 0;
 return 1;
}

static const char *string_prop(const void *fdt, int node, const char *key)
{
 int len;
 const char *s = fdt_getprop(fdt, node, key, &len);
 if (!s || len < 2 || s[len - 1] || strlen(s) != (size_t)len - 1) return NULL;
 return s;
}

static int referenced_image(const void *fit, int conf, const char *key)
{
 const char *name = string_prop(fit, conf, key);
 int images = fdt_path_offset(fit, "/images");
 return name && images >= 0 ? fdt_subnode_offset(fit, images, name) : -1;
}

static int property_is(const void *fdt, int node, const char *key, const char *want)
{
 const char *s = string_prop(fdt, node, key);
 return s && !strcmp(s, want);
}

static uint64_t le64(const uint8_t *p)
{
 uint64_t v = 0;
 int i;
 for (i = 7; i >= 0; i--) v = (v << 8) | p[i];
 return v;
}

static int verify_hashes(const void *fit, int node, const void *data, size_t size)
{
 const uint8_t *bytes = data;
 const void *value;
 const char *algo;
 uint8_t sum[SHA256_DIGEST_LENGTH];
 uint32_t crc;
 int child, len, checked = 0, bit;
 size_t i;
 for (child = fdt_first_subnode(fit, node); child >= 0;
      child = fdt_next_subnode(fit, child)) {
  algo = string_prop(fit, child, "algo");
  if (!algo) continue;
  value = fdt_getprop(fit, child, "value", &len);
  if (!value || len <= 0) return 0;
  if (!strcmp(algo, "sha1")) {
   SHA1(data, size, sum);
   if (len != SHA_DIGEST_LENGTH || memcmp(value, sum, len)) return 0;
  } else if (!strcmp(algo, "sha256")) {
   SHA256(data, size, sum);
   if (len != SHA256_DIGEST_LENGTH || memcmp(value, sum, len)) return 0;
  } else if (!strcmp(algo, "crc32")) {
   crc = ~0u;
   for (i = 0; i < size; i++) {
    crc ^= bytes[i];
    for (bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
   }
   crc = ~crc;
   if (len != 4 || fdt32_to_cpu(*(const fdt32_t *)value) != crc) return 0;
  } else continue;
  checked++;
 }
 return checked > 0;
}

static int check(const uint8_t *fit, size_t size)
{
 const void *dt, *data;
 const char *name;
 const fdt32_t *reg, *cells;
 size_t off, end;
 uint64_t used;
 int configs, conf, kernel, fdt_node, dt_len, len, part, parent;

 if (!bounds_fdt(fit, size)) return 1;
 configs = fdt_path_offset(fit, "/configurations");
 if (configs < 0 || !(name = string_prop(fit, configs, "default"))) return 2;
 conf = fdt_subnode_offset(fit, configs, name);
 if (conf < 0) return 2;
 kernel = referenced_image(fit, conf, "kernel");
 if (kernel < 0 || !property_is(fit, kernel, "arch", "mips") ||
     !property_is(fit, kernel, "os", "linux") ||
     !property_is(fit, kernel, "type", "kernel")) return 3;
 data = fdt_getprop(fit, kernel, "data", &len);
 if (!data || len <= 0 || !verify_hashes(fit, kernel, data, (size_t)len)) return 3;
 fdt_node = referenced_image(fit, conf, "fdt");
 if (fdt_node < 0) return 4;
 dt = fdt_getprop(fit, fdt_node, "data", &dt_len);
 if (!dt || dt_len <= 0 || !verify_hashes(fit, fdt_node, dt, (size_t)dt_len) ||
     !bounds_fdt(dt, (size_t)dt_len)) return 4;
 if (fdt_node_check_compatible(dt, 0, "mediatek,mt7621da-5port") ||
     !fdt_node_check_compatible(dt, 0, "mediatek,mt7621da-5port-16m")) return 5;
 part = fdt_node_offset_by_prop_value(dt, -1, "label", "firmware", 9);
 if (part < 0 || fdt_node_offset_by_prop_value(dt, part, "label", "firmware", 9) >= 0) return 6;
 parent = fdt_parent_offset(dt, part);
 cells = fdt_getprop(dt, parent, "#address-cells", &len);
 if (!cells || len != 4 || fdt32_to_cpu(*cells) != 1) return 6;
 cells = fdt_getprop(dt, parent, "#size-cells", &len);
 if (!cells || len != 4 || fdt32_to_cpu(*cells) != 1) return 6;
 reg = fdt_getprop(dt, part, "reg", &len);
 if (!reg || len != 8 || fdt32_to_cpu(reg[0]) != FW_OFFSET ||
     fdt32_to_cpu(reg[1]) != FW_SIZE) return 6;
 off = (fdt_totalsize(fit) + ERASE_SIZE - 1) & ~(ERASE_SIZE - 1);
 if (off > size || size - off < 96 || memcmp(fit + off, "hsqs", 4)) return 7;
 if (fit[off + 28] != 4 || fit[off + 29] ||
     fit[off + 20] != 4 || fit[off + 21]) return 7;
 used = le64(fit + off + 40);
 if (used < 96 || used > size - off) return 7;
 end = (off + (size_t)used + ERASE_SIZE - 1) & ~(ERASE_SIZE - 1);
 if (end >= FW_SIZE) return 8;
 printf("8MiB layout OK: squashfs=%llu overlay_partition=%zu\n",
        (unsigned long long)used, (size_t)FW_SIZE - end);
 return 0;
}

int main(int argc, char **argv)
{
 struct stat st;
 void *p;
 int fd, ret;
 if (argc != 2) return 64;
 fd = open(argv[1], O_RDONLY);
 if (fd < 0) return 65;
 if (fstat(fd, &st) || st.st_size < 96 || st.st_size > FW_SIZE) {
  close(fd); return 66;
 }
 p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
 close(fd);
 if (p == MAP_FAILED) return 67;
 ret = check(p, (size_t)st.st_size);
 munmap(p, (size_t)st.st_size);
 if (ret) fprintf(stderr, "Invalid MT7621DA 8MiB image layout (%d)\n", ret);
 return ret;
}
