#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMBER_LINES 1          // -n
#define NUMBER_NONBLANK_LINES 2 // -b
#define SQUEEZE_BLANK_LINES 1   // -s
#define SHOW_ENDS 1             // -E

typedef struct {
  unsigned int number : 2;
  unsigned int squeeze_blank : 1;
  unsigned int show_ends : 1;
} Flags;

#define BUFFER_MAX_CAPACITY SIZE_MAX

typedef struct {
  size_t capacity;
  size_t length;
  size_t line_count;
  char *data;
} Buffer;

void check_fail(int, const char *, ...);

void usage_print(void);

void output_append_stdin(Buffer *, Flags *);
void output_append_file(Buffer *, Flags *, const char *);
void output_append_with_flags(Buffer *, Flags *, char *);
void buffer_grow(Buffer *, size_t);

void flag_set(Flags *, const char *);

int main(int argc, char **argv) {
  Buffer out = {0};
  buffer_grow(&out, 256);

  Flags flags = {0};

  int flag_count = 0;
  for (int i = 1; i < argc; i++) {
    if (strlen(argv[i]) != 1 && *argv[i] == '-') {
      flag_set(&flags, argv[i]);
      flag_count++;
    }
  }

  if (flag_count == argc - 1) {
    output_append_stdin(&out, &flags);
  } else {
    for (int i = 1; i < argc; i++) {
      if (*argv[i] == '-') {
        if (strlen(argv[i]) == 1) {
          output_append_stdin(&out, &flags);
        } else {
          continue;
        }
      } else {
        output_append_file(&out, &flags, argv[i]);
      }
    }
  }

  fprintf(stdout, "%s", out.data);

  free(out.data);
  return 0;
}

void check_fail(int is_fail, const char *err, ...) {
  if (!is_fail) {
    return;
  }
  va_list args;
  va_start(args, err);

  vfprintf(stderr, err, args);

  va_end(args);
  exit(EXIT_FAILURE);
}

void usage_print(void) {
  fprintf(stderr, "Usage: mycat [OPTIONS]... <file>...\n");
  exit(EXIT_FAILURE);
}

void flag_set(Flags *flags, const char *flag_str) {
  if (strcmp(flag_str, "-n") == 0) {
    flags->number = NUMBER_LINES;
  } else if (strcmp(flag_str, "-b") == 0) {
    flags->number = NUMBER_NONBLANK_LINES;
  } else if (strcmp(flag_str, "-E") == 0) {
    flags->show_ends = SHOW_ENDS;
  } else if (strcmp(flag_str, "-s") == 0) {
    flags->squeeze_blank = SQUEEZE_BLANK_LINES;
  } else {
    check_fail(1, "ERROR: invalid flag '%s'\n", flag_str);
  }
}

void buffer_grow(Buffer *buf, size_t capacity) {
  check_fail(capacity > BUFFER_MAX_CAPACITY,
             "ERROR: buffer max capacity of '%zu' reached\n",
             BUFFER_MAX_CAPACITY);
  buf->data = realloc(buf->data, capacity);
  check_fail(buf->data == NULL, "ERROR: failed to allocate memory\n");
  buf->capacity = capacity;
}

void output_append_stdin(Buffer *out, Flags *flags) {
  Buffer data = {0};
  buffer_grow(&data, 256);
  char c;
  while ((c = fgetc(stdin)) != EOF) {
    if (data.length == data.capacity) {
      buffer_grow(&data, data.capacity * 2 + 1);
    }

    data.data[data.length] = c;
    data.length++;
  }

  if (data.length == 0) {
    usage_print();
  }

  data.data[data.length] = '\0';

  output_append_with_flags(out, flags, data.data);
  free(data.data);
}

void output_append_file(Buffer *out, Flags *flags, const char *file_path) {
  FILE *file = fopen(file_path, "r");
  check_fail(file == NULL, "ERROR: reading file '%s'\n", file_path);

  check_fail(fseek(file, 0, SEEK_END) != 0, "ERROR: reading file '%s'\n",
             file_path);
  long length = ftell(file);
  if (length == 0) {
    out->data[out->length] = '\0';
    return;
  }
  check_fail(fseek(file, 0, SEEK_SET) != 0,
             "ERROR: file '%s' changed during execution\n", file_path);

  char *data = malloc(length + 1);
  check_fail(data == NULL, "ERROR: failed to allocate memory for file '%s'\n",
             file_path);

  size_t bytes_read = fread(data, 1, length, file);
  check_fail(bytes_read - length != 0,
             "ERROR: file '%s' changed during execution\n", file_path);
  data[length] = '\0';

  output_append_with_flags(out, flags, data);
  free(data);

  fclose(file);
}

void output_append_with_flags(Buffer *out, Flags *flags, char *data) {
  while (*data != '\0') {
    if (flags->squeeze_blank == SQUEEZE_BLANK_LINES && *data == '\n') {
      while (*data == '\n') {
        data = data + 1;
      }
      data = data - 1;
    }

    char *line_end = strchr(data, '\n');

    size_t line_len = line_end - data + 1;

    if (flags->number == NUMBER_LINES ||
        (flags->number == NUMBER_NONBLANK_LINES && line_len > 1)) {
      char line_count_str[128];
      sprintf(line_count_str, "%zu\t", out->line_count + 1);
      size_t line_count_str_len = strlen(line_count_str);

      if (out->capacity < out->length + line_count_str_len) {
        buffer_grow(out, out->capacity * 2 + line_count_str_len + 1);
      }

      strcpy(out->data + out->length, line_count_str);
      out->length += line_count_str_len;
      out->line_count++;
    }

    if (out->capacity < out->length + line_len) {
      buffer_grow(out, out->capacity * 2 + line_len + 1);
    }

    memcpy(out->data + out->length, data, line_len);
    out->length += line_len;

    if (flags->show_ends == 1) {
      if (out->capacity < out->length + 1) {
        buffer_grow(out, out->capacity * 2 + 1);
      }
      out->data[out->length - 1] = '$';
      out->data[out->length] = '\n';
      out->length++;
    }

    data = data + line_len;
  }

  out->data[out->length] = '\0';
}
