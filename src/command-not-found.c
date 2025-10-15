#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define _(string) gettext(string)
#define MAX_CMD_LEN 512
#define MAX_INPUT_LEN 100
#define CACHE_FILE "/tmp/apt_cache_descriptions.txt"
#define CACHE_MAX_AGE 86400

/* Структура для сопоставления русских и английских символов раскладки */
typedef struct {
    const char *ru_char;
    char en_char;
} LayoutMap;

/* Таблица преобразования русских символов в английские (раскладка клавиатуры) */
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

/* Определяет длину UTF-8 символа в байтах */
int utf8_char_len(const char *str) {
    unsigned char c = (unsigned char)str[0];
    if ((c & 0x80) == 0) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Копирует один UTF-8 символ из src в dest */
void utf8_char_copy(char *dest, const char *src) {
    int len = utf8_char_len(src);
    strncpy(dest, src, len);
    dest[len] = '\0';
}

/* Преобразует русский символ в соответствующий английский по раскладке */
char convert_ru_to_en(const char *ru_char) {
    for (int i = 0; i < map_size; i++) {
        if (strcmp(ru_char, ru_to_en_map[i].ru_char) == 0) {
            return ru_to_en_map[i].en_char;
        }
    }
    return ru_char[0];
}

/* Конвертирует команду из русской раскладки в английскую */
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

/* Безопасная версия snprintf с ограничением длины аргумента */
int safe_snprintf(char *buf, size_t size, const char *format, const char *arg) {
    if (strlen(arg) > MAX_INPUT_LEN) {
        char trimmed_arg[MAX_INPUT_LEN + 1];
        strncpy(trimmed_arg, arg, MAX_INPUT_LEN);
        trimmed_arg[MAX_INPUT_LEN] = '\0';
        return snprintf(buf, size, format, trimmed_arg);
    }
    return snprintf(buf, size, format, arg);
}

/* Проверяет существование команды в PATH */
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

/* Проверяет существование команды в системных каталогах */
int command_exists_in_system_bin(const char *cmd) {
    const char *system_dirs[] = {
        "/bin", "/sbin", "/usr/bin", "/usr/sbin", 
        "/usr/local/bin", "/usr/local/sbin", NULL
    };
    
    for (int i = 0; system_dirs[i] != NULL; i++) {
        char full_path[MAX_CMD_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", system_dirs[i], cmd);
        
        if (access(full_path, F_OK) == 0) {
            return 1;
        }
    }

    char cmd_which[MAX_CMD_LEN];
    snprintf(cmd_which, sizeof(cmd_which), "which %s >/dev/null 2>&1", cmd);
    
    if (system(cmd_which) == 0) {
        return 1;
    }
    
    return 0;
}

/* Проверяет установлен ли пакет через rpm */
int package_is_installed(const char *package_name) {
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "rpm -q %s >/dev/null 2>&1", package_name);
    return system(cmd) == 0;
}

/* Проверяет актуальность кэша пакетов */
int cache_is_valid() {
    struct stat st;
    if (stat(CACHE_FILE, &st) != 0) {
        return 0;
    }
    
    time_t now = time(NULL);
    time_t file_age = now - st.st_mtime;
    
    return (file_age < CACHE_MAX_AGE);
}

/* Создает кэш описаний пакетов через apt-cache */
void create_cache() {
    printf("Creating apt-cache database... (this may take a moment)\n");
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "apt-cache search . > %s 2>/dev/null", CACHE_FILE);
    int result = system(cmd);
    if (result == 0) {
        printf("Cache created successfully at %s\n", CACHE_FILE);
    } else {
        printf("Failed to create cache. Please check if apt-cache is available.\n");
    }
}

/* Принудительно пересоздает кэш */
void force_recreate_cache() {
    printf("Forcing cache recreation...\n");
    unlink(CACHE_FILE);
    create_cache();
}

/* Ищет точное совпадение пакета в кэше */
int search_exact_package(const char *package_name, char *result, size_t result_size) {
    if (!cache_is_valid()) {
        create_cache();
    }
    
    FILE *fp = fopen(CACHE_FILE, "r");
    if (!fp) {
        return 0;
    }
    
    char line[MAX_CMD_LEN];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t name_len = strlen(package_name);
        if (strncmp(line, package_name, name_len) == 0) {
            char next_char = line[name_len];
            if (next_char == ' ' || next_char == '\t' || next_char == '\n' || next_char == '\0') {
                strncpy(result, line, result_size - 1);
                result[result_size - 1] = '\0';
                found = 1;
                break;
            }
        }
    }
    
    fclose(fp);
    return found;
}

/* Ищет похожие пакеты в кэше по шаблону */
int search_similar_in_cache(const char *pattern, char *results[], int max_results) {
    if (!cache_is_valid()) {
        create_cache();
    }
    
    FILE *fp = fopen(CACHE_FILE, "r");
    if (!fp) {
        return 0;
    }

    char *all_results[100];
    int all_count = 0;
    char line[MAX_CMD_LEN];
    
    while (fgets(line, sizeof(line), fp) != NULL && all_count < 100) {
        char *first_space = strchr(line, ' ');
        if (first_space != NULL) {
            size_t package_name_len = first_space - line;
            char package_name[256];
            
            if (package_name_len >= sizeof(package_name)) {
                package_name_len = sizeof(package_name) - 1;
            }
            
            strncpy(package_name, line, package_name_len);
            package_name[package_name_len] = '\0';

            if (strstr(package_name, pattern) != NULL) {
                all_results[all_count] = strdup(line);
                if (all_results[all_count] == NULL) break;
                all_count++;
            }
        }
    }
    
    fclose(fp);
    
    if (all_count == 0) {
        return 0;
    }

    /* Сортировка результатов по длине имени пакета */
    for (int i = 0; i < all_count - 1; i++) {
        for (int j = i + 1; j < all_count; j++) {
            char *first_space_i = strchr(all_results[i], ' ');
            char *first_space_j = strchr(all_results[j], ' ');
            
            if (first_space_i != NULL && first_space_j != NULL) {
                size_t len_i = first_space_i - all_results[i];
                size_t len_j = first_space_j - all_results[j];

                if (len_i > len_j) {
                    char *temp = all_results[i];
                    all_results[i] = all_results[j];
                    all_results[j] = temp;
                }
            }
        }
    }

    int count = (all_count < max_results) ? all_count : max_results;
    for (int i = 0; i < count; i++) {
        results[i] = all_results[i];
    }

    for (int i = count; i < all_count; i++) {
        free(all_results[i]);
    }
    
    return count;
}

/* Выводит справку по использованию программы */
void print_usage() {
    printf("Usage:\n");
    printf("  command-not-found <command>    - Search for a command and suggest packages\n");
    printf("  command-not-found --rebase     - Force recreate the apt-cache database\n");
    printf("  command-not-found --help       - Show this help message\n");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    setlocale(LC_MESSAGES, "");

    bindtextdomain("command-not-found", "/usr/share/locale/");
    textdomain("command-not-found");
    
    /* Обработка аргументов командной строки */
    if (argc == 2) {
        if (strcmp(argv[1], "--rebase") == 0 || strcmp(argv[1], "-r") == 0) {
            force_recreate_cache();
            return 0;
        }
        else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("command-not-found with cache optimization\n");
            return 0;
        }
    }
    
    if (argc < 2) {
        print_usage();
        return 1;
    }

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
    
    /* Попытка автокоррекции раскладки клавиатуры */
    convert_command(converted_cmd);
    if (strcmp(original_cmd, converted_cmd) != 0) {
        if (command_exists_in_path(converted_cmd)) {
            printf("Auto-correcting '%s' to '%s' and executing:\n", original_cmd, converted_cmd);
            execlp(converted_cmd, converted_cmd, NULL);
            perror("exec failed");
            return 1;
        }
    }

    /* Попытка выполнения оригинальной команды */
    if (command_exists_in_path(original_cmd)) {
        execlp(original_cmd, original_cmd, NULL);
        perror("exec failed");
        return 1;
    }

    /* Проверка команды в системных каталогах */
    if (command_exists_in_system_bin(original_cmd)) {
        printf("%s: %s\n", original_cmd, _("Command found in system directories but not in your PATH"));
        printf("%s 'sudo %s'\n", _("Try running with sudo:"), original_cmd);
        return 127;
    }

    /* Проверка автокорректированной команды в системных каталогах */
    if (strcmp(original_cmd, converted_cmd) != 0 && command_exists_in_system_bin(converted_cmd)) {
        printf("%s '%s'?\n", _("Did you mean"), converted_cmd);
        printf("%s 'sudo %s'\n", _("Try running with sudo:"), converted_cmd);
        return 127;
    }

    printf("%s: %s\n", original_cmd, _("Command not found"));
    fflush(stdout);

    char output[MAX_CMD_LEN];
    int found = 0;

    /* Поиск пакета содержащего команду */
    if (search_exact_package(converted_cmd, output, sizeof(output))) {
        char *first_space = strchr(output, ' ');
        if (first_space != NULL) {
            size_t package_len = first_space - output;
            char package_name[256];
            
            if (package_len >= sizeof(package_name)) {
                package_len = sizeof(package_name) - 1;
            }
            
            strncpy(package_name, output, package_len);
            package_name[package_len] = '\0';

            /* Удаление пробельных символов в конце имени пакета */
            char *end = package_name + strlen(package_name) - 1;
            while (end > package_name && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
    if (package_is_installed(package_name)) {
        printf("%s\n", _("Package is already installed but command not found."));
            } else {
                printf("sudo apt-get install %s\n", package_name);
            }
            found = 1;
        }
    }

    /* Поиск похожих пакетов если точное совпадение не найдено */
    if (!found) {
        printf("%s\n", _("Perhaps you were looking for:"));
        
        char *results[3];
        int result_count = search_similar_in_cache(converted_cmd, results, 3);
        
        if (result_count > 0) {
            for (int i = 0; i < result_count; i++) {
                printf("%s", results[i]);
                free(results[i]);
            }
        } else {
            printf("Try: apt-cache search %s\n", converted_cmd);
        }
    }

    return 127;
}