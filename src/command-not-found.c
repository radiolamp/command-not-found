#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <unistd.h>

#define _(string) gettext(string)

#define MAX_CMD_LEN 512
#define MAX_INPUT_LEN 100

typedef struct {
    const char *ru_char;
    char en_char;
} LayoutMap;

const LayoutMap ru_to_en_map[] = {
    {"а", 'f'}, {"б", ','}, {"в", 'd'}, {"г", 'u'}, {"д", 'l'}, {"е", 't'}, {"ё", '`'}, 
    {"ж", ';'}, {"з", 'p'}, {"и", 'b'}, {"й", 'q'}, {"к", 'r'}, {"л", 'k'}, {"м", 'v'}, 
    {"н", 'y'}, {"о", 'j'}, {"п", 'g'}, {"р", 'h'}, {"с", 'c'}, {"т", 'n'}, {"у", 'e'}, 
    {"ф", 'a'}, {"х", '['}, {"ц", 'w'}, {"ч", 'x'}, {"ш", 'i'}, {"щ", 'o'}, {"ъ", ']'}, 
    {"ы", 's'}, {"ь", 'm'}, {"э", '\''}, {"ю", '.'}, {"я", 'z'},
    {"А", 'F'}, {"Б", '<'}, {"В", 'D'}, {"Г", 'U'}, {"Д", 'L'}, {"Е", 'T'}, {"Ё", '~'}, 
    {"Ж", ':'}, {"З", 'P'}, {"И", 'B'}, {"Й", 'Q'}, {"К", 'R'}, {"Л", 'K'}, {"М", 'V'}, 
    {"Н", 'Y'}, {"О", 'J'}, {"П", 'G'}, {"Р", 'H'}, {"С", 'C'}, {"Т", 'N'}, {"У", 'E'}, 
    {"Ф", 'A'}, {"Х", '{'}, {"Ц", 'W'}, {"Ч", 'X'}, {"Ш", 'I'}, {"Щ", 'O'}, {"Ъ", '}'}, 
    {"Ы", 'S'}, {"Ь", 'M'}, {"Э", '\"'}, {"Ю", '>'}, {"Я", 'Z'}
};

const int map_size = sizeof(ru_to_en_map) / sizeof(ru_to_en_map[0]);

int utf8_strcmp(const char *str1, const char *str2) {
    return strcmp(str1, str2);
}

int utf8_char_len(const char *str) {
    unsigned char c = (unsigned char)str[0];
    if ((c & 0x80) == 0) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

void utf8_char_copy(char *dest, const char *src) {
    int len = utf8_char_len(src);
    strncpy(dest, src, len);
    dest[len] = '\0';
}

char convert_ru_to_en(const char *ru_char) {
    for (int i = 0; i < map_size; i++) {
        if (utf8_strcmp(ru_char, ru_to_en_map[i].ru_char) == 0) {
            return ru_to_en_map[i].en_char;
        }
    }
    return ru_char[0];
}

void convert_command(char* cmd) {
    char converted[MAX_CMD_LEN] = {0};
    int conv_index = 0;
    
    for (int i = 0; cmd[i] != '\0' && conv_index < MAX_INPUT_LEN - 1; ) {
        int char_len = utf8_char_len(&cmd[i]);
        
        if (char_len > 1) {
            char utf8_char[5] = {0};
            utf8_char_copy(utf8_char, &cmd[i]);
            
            char en_char = convert_ru_to_en(utf8_char);
            converted[conv_index++] = en_char;
            i += char_len;
        } else {
            converted[conv_index++] = cmd[i];
            i++;
        }
    }
    
    converted[conv_index] = '\0';
    strcpy(cmd, converted);
}

int safe_snprintf(char *buf, size_t size, const char *format, const char *arg) {
    if (strlen(arg) > MAX_INPUT_LEN) {
        char trimmed_arg[MAX_INPUT_LEN + 1];
        strncpy(trimmed_arg, arg, MAX_INPUT_LEN);
        trimmed_arg[MAX_INPUT_LEN] = '\0';
        return snprintf(buf, size, format, trimmed_arg);
    }
    return snprintf(buf, size, format, arg);
}

int command_exists_in_path(const char *cmd) {
    char *path = getenv("PATH");
    if (!path) return 0;

    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");
    
    int found = 0;
    while (dir != NULL) {
        char full_path[MAX_CMD_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        
        if (access(full_path, X_OK) == 0) {
            found = 1;
            break;
        }
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return found;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    setlocale(LC_MESSAGES, "");

    bindtextdomain("command-not-found", "/usr/share/locale/");
    textdomain("command-not-found");
    if (argc < 2) return 1;

    if (strlen(argv[1]) > MAX_INPUT_LEN) {
        fprintf(stderr, "Input command too long\n");
        return 1;
    }

    char converted_cmd[MAX_INPUT_LEN + 1];
    strncpy(converted_cmd, argv[1], MAX_INPUT_LEN);
    converted_cmd[MAX_INPUT_LEN] = '\0';
    
    char original_cmd[MAX_INPUT_LEN + 1];
    strncpy(original_cmd, argv[1], MAX_INPUT_LEN);
    original_cmd[MAX_INPUT_LEN] = '\0';
    
    convert_command(converted_cmd);

    if (strcmp(original_cmd, converted_cmd) != 0) {
        if (command_exists_in_path(converted_cmd)) {
            printf("Auto-correcting '%s' to '%s' and executing:\n", original_cmd, converted_cmd);
            execlp(converted_cmd, converted_cmd, NULL);
            perror("exec failed");
            return 1;
        } else {
            printf("%s '%s'? ", _("Did you mean"), converted_cmd);
        }
    }

    if (command_exists_in_path(original_cmd)) {
        execlp(original_cmd, original_cmd, NULL);
        perror("exec failed");
        return 1;
    }

    printf("%s: %s\n", original_cmd, _("Command not found"));
    fflush(stdout);

    char cmd[MAX_CMD_LEN];
    char output[MAX_CMD_LEN];
    FILE *fp;
    int found = 0;

    if (safe_snprintf(cmd, sizeof(cmd), "apt-cache search '^%s$' 2>/dev/null", converted_cmd) >= (int)sizeof(cmd)) {
        fprintf(stderr, "Command too long\n");
        return 1;
    }

    fp = popen(cmd, "r");
    if (fp != NULL) {
        if (fgets(output, sizeof(output), fp) != NULL) {
            char *dash_pos = strchr(output, '-');
            if (dash_pos != NULL) {
                size_t package_len = dash_pos - output;
                char package_name[256];
                
                if (package_len >= sizeof(package_name)) {
                    package_len = sizeof(package_name) - 1;
                }
                
                strncpy(package_name, output, package_len);
                package_name[package_len] = '\0';

                char *end = package_name + strlen(package_name) - 1;
                while (end > package_name && (*end == ' ' || *end == '\t' || *end == '\n')) {
                    *end = '\0';
                    end--;
                }
                
                printf("sudo apt-get install %s\n", package_name);
                found = 1;
            }
        }
        pclose(fp);
    }

    if (!found) {
        printf("%s\n", _("Perhaps you were looking for:"));
        
        if (safe_snprintf(cmd, sizeof(cmd), 
            "apt-cache search '(/(bin|sbin)|/usr/(sbin|bin))/%s' 2>/dev/null | head -3", 
            converted_cmd) >= (int)sizeof(cmd)) {
            printf("Try: apt-cache search %s\n", converted_cmd);
        } else {
            fp = popen(cmd, "r");
            
            if (fp != NULL) {
                int count = 0;
                while (fgets(output, sizeof(output), fp) != NULL && count < 3) {
                    printf("%s", output);
                    count++;
                }
                pclose(fp);
                
                if (count == 0) {
                    printf("Try: apt-cache search %s\n", converted_cmd);
                }
            } else {
                printf("Try: apt-cache search %s\n", converted_cmd);
            }
        }
    }

    return 127;
}
