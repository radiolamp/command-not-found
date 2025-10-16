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
#include <dirent.h>

/* Макрос для интернационализации */
#define _(string) gettext(string)
/* Максимальная длина команды */
#define MAX_CMD_LEN 2048
/* Максимальная длина ввода пользователя */
#define MAX_INPUT_LEN 100
/* Максимальная длина пути */
#define MAX_PATH_LEN 1024

/* Структура для сопоставления русских и английских символов раскладки */
typedef struct {
    const char *ru_char;  // Русский символ в UTF-8
    char en_char;         // Соответствующий английский символ
} LayoutMap;

/* Таблица преобразования русской раскладки в английскую */
const LayoutMap ru_to_en_map[] = {
    {"а", 'f'}, {"б", ','}, {"в", 'd'}, {"г", 'u'}, {"д", 'l'}, {"е", 't'}, {"ё", '`'}, 
    {"ж", ';'}, {"з", 'p'}, {"и", 'b'}, {"й", 'q'}, {"к", 'r'}, {"л", 'k'}, {"м", 'v'}, 
    {"н", 'y'}, {"о", 'j'}, {"п", 'g'}, {"р", 'h'}, {"с", 'c'}, {"т", 'n'}, {"у", 'e'}, 
    {"ф", 'a'}, {"х", '['}, {"ц", 'w'}, {"ч", 'x'}, {"ш", 'i'}, {"щ", 'o'}, {"ъ", ']'}, 
    {"ы", 's'}, {"ь", 'm'}, {"э", '\''}, {"ю", '.'}, {"я", 'z'},
    /* Заглавные буквы */
    {"А", 'F'}, {"Б", '<'}, {"В", 'D'}, {"Г", 'U'}, {"Д", 'L'}, {"Е", 'T'}, {"Ё", '~'}, 
    {"Ж", ':'}, {"З", 'P'}, {"И", 'B'}, {"Й", 'Q'}, {"К", 'R'}, {"Л", 'K'}, {"М", 'V'}, 
    {"Н", 'Y'}, {"О", 'J'}, {"П", 'G'}, {"Р", 'H'}, {"С", 'C'}, {"Т", 'N'}, {"У", 'E'}, 
    {"Ф", 'A'}, {"Х", '{'}, {"Ц", 'W'}, {"Ч", 'X'}, {"Ш", 'I'}, {"Щ", 'O'}, {"Ъ", '}'}, 
    {"Ы", 'S'}, {"Ь", 'M'}, {"Э", '\"'}, {"Ю", '>'}, {"Я", 'Z'}
};

/* Размер таблицы преобразования */
const int map_size = sizeof(ru_to_en_map) / sizeof(ru_to_en_map[0]);

/* Структура для хранения информации о пакете */
typedef struct {
    char package_name[256];    // Название пакета
    char binary_path[256];     // Путь к бинарному файлу
    char description[512];     // Описание пакета
} PackageInfo;

/* 
 * Определяет длину UTF-8 символа
 * Возвращает количество байт, занимаемых символом
 */
int utf8_char_len(const char *str) {
    unsigned char c = (unsigned char)str[0];
    if ((c & 0x80) == 0) return 1;        // 1-байтовый символ (0xxxxxxx)
    else if ((c & 0xE0) == 0xC0) return 2; // 2-байтовый символ (110xxxxx)
    else if ((c & 0xF0) == 0xE0) return 3; // 3-байтовый символ (1110xxxx)
    else if ((c & 0xF8) == 0xF0) return 4; // 4-байтовый символ (11110xxx)
    return 1; // По умолчанию 1 байт
}

/* 
 * Копирует один UTF-8 символ из src в dest
 * dest должен иметь размер не менее 5 байт
 */
void utf8_char_copy(char *dest, const char *src) {
    int len = utf8_char_len(src);
    strncpy(dest, src, len);
    dest[len] = '\0'; // Завершающий нуль
}

/* 
 * Преобразует русский символ в соответствующий английский
 * Возвращает английский символ или оригинал, если сопоставление не найдено
 */
char convert_ru_to_en(const char *ru_char) {
    for (int i = 0; i < map_size; i++) {
        if (strcmp(ru_char, ru_to_en_map[i].ru_char) == 0) {
            return ru_to_en_map[i].en_char;
        }
    }
    return ru_char[0]; // Возвращаем первый байт, если символ не найден
}

/* 
 * Преобразует команду из русской раскладки в английскую
 * Обрабатывает многобайтовые UTF-8 символы
 */
void convert_command(char* cmd) {
    char converted[MAX_CMD_LEN] = {0}; // Буфер для преобразованной команды
    int conv_index = 0; // Индекс в буфере converted
    
    // Обрабатываем команду посимвольно (с учетом UTF-8)
    for (int i = 0; cmd[i] != '\0' && conv_index < MAX_INPUT_LEN - 1; ) {
        int char_len = utf8_char_len(&cmd[i]);
        
        if (char_len > 1) {
            // Многобайтовый UTF-8 символ (русская буква)
            char utf8_char[5] = {0};
            utf8_char_copy(utf8_char, &cmd[i]);
            
            // Преобразуем русский символ в английский
            char en_char = convert_ru_to_en(utf8_char);
            converted[conv_index++] = en_char;
            i += char_len; // Пропускаем все байты UTF-8 символа
        } else {
            // Однобайтовый символ (английская буква или другой символ)
            converted[conv_index++] = cmd[i];
            i++;
        }
    }
    
    converted[conv_index] = '\0'; // Завершаем строку
    strcpy(cmd, converted); // Копируем обратно в оригинальный буфер
}

/* 
 * Проверяет существование команды в путях PATH
 * Возвращает 1 если команда найдена, 0 если нет
 */
int command_exists_in_path(const char *cmd) {
    char *path = getenv("PATH");
    if (!path) return 0;

    char *path_copy = strdup(path); // Копируем PATH для безопасного разбора
    char *dir = strtok(path_copy, ":");
    
    int found = 0;
    // Проверяем каждый каталог в PATH
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

/* 
 * Проверяет существование команды в системных каталогах
 * Использует предопределенный список системных каталогов и команду which
 */
int command_exists_in_system_bin(const char *cmd) {
    // Список системных каталогов для поиска
    const char *system_dirs[] = {
        "/bin", "/sbin", "/usr/bin", "/usr/sbin", 
        "/usr/local/bin", "/usr/local/sbin", NULL
    };
    
    // Проверяем в каждом системном каталоге
    for (int i = 0; system_dirs[i] != NULL; i++) {
        char full_path[MAX_CMD_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", system_dirs[i], cmd);
        
        if (access(full_path, F_OK) == 0) {
            return 1;
        }
    }

    // Дополнительная проверка с помощью команды which
    char cmd_which[MAX_CMD_LEN];
    snprintf(cmd_which, sizeof(cmd_which), "which %s >/dev/null 2>&1", cmd);
    
    if (system(cmd_which) == 0) {
        return 1;
    }
    
    return 0;
}

/* 
 * Проверяет установлен ли пакет в системе
 * Использует команду rpm для проверки
 */
int package_is_installed(const char *package_name) {
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "rpm -q %s >/dev/null 2>&1", package_name);
    return system(cmd) == 0; // Возвращает 1 если пакет установлен
}

/* 
 * Проверяет существует ли пакет уже в массиве результатов
 * Используется для избежания дубликатов при поиске
 */
int package_already_exists(const PackageInfo *packages, int count, const char *package_name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(packages[i].package_name, package_name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 
 * Функция сравнения для qsort
 * Сравнивает пакеты по длине имени, затем по алфавиту
 */
int compare_package_by_name_length(const void *a, const void *b) {
    const PackageInfo *pkg_a = (const PackageInfo *)a;
    const PackageInfo *pkg_b = (const PackageInfo *)b;
    
    int len_a = strlen(pkg_a->package_name);
    int len_b = strlen(pkg_b->package_name);
    
    // Сначала сравниваем по длине имени
    if (len_a != len_b) {
        return len_a - len_b;
    }
    
    // При равной длине - по алфавиту
    return strcmp(pkg_a->package_name, pkg_b->package_name);
}

/* 
 * Ищет пакет, содержащий указанную команду
 * Использует pkglist-query для поиска в базе пакетов
 * Возвращает 1 если пакет найден, 0 если нет
 */
int find_package_for_command(const char *command_name, PackageInfo *result) {
    DIR *dir;
    struct dirent *entry;
    char *pkglist_dir = "/var/lib/apt/lists"; // Каталог с информацией о пакетах
    int found = 0;
    
    dir = opendir(pkglist_dir);
    if (dir == NULL) {
        return 0;
    }
    
    // Читаем все файлы в каталоге pkglist
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "pkglist.") != NULL) {
            char filepath[MAX_PATH_LEN];
            snprintf(filepath, sizeof(filepath), "%s/%s", pkglist_dir, entry->d_name);
            
            // Формируем команду для запроса информации о пакете
            char cmd[MAX_CMD_LEN];
            int needed = snprintf(cmd, sizeof(cmd), 
                     "pkglist-query \"[%%{FILENAMES}\\t%%{NAME}\\t%%{SUMMARY}\\n]\" \"%s\" 2>/dev/null", 
                     filepath);
            
            if (needed >= (int)sizeof(cmd)) {
                continue; // Пропускаем если команда слишком длинная
            }
            
            // Выполняем команду и читаем вывод
            FILE *fp = popen(cmd, "r");
            if (fp == NULL) {
                continue;
            }
            
            char line[MAX_CMD_LEN];
            while (fgets(line, sizeof(line), fp) != NULL) {
                // Разбираем строку: путь, имя пакета, описание
                char *path = strtok(line, "\t");
                char *package = strtok(NULL, "\t");
                char *description = strtok(NULL, "\n");
                
                if (path != NULL && package != NULL && description != NULL) {
                    // Извлекаем имя файла из полного пути
                    char *filename = strrchr(path, '/');
                    if (filename != NULL) {
                        filename++;
                        
                        // Проверяем совпадение имени команды
                        if (strcmp(filename, command_name) == 0) {
                            // Заполняем структуру результата
                            strncpy(result->package_name, package, sizeof(result->package_name) - 1);
                            strncpy(result->binary_path, path, sizeof(result->binary_path) - 1);
                            strncpy(result->description, description, sizeof(result->description) - 1);
                            
                            // Гарантируем завершение строк
                            result->package_name[sizeof(result->package_name) - 1] = '\0';
                            result->binary_path[sizeof(result->binary_path) - 1] = '\0';
                            result->description[sizeof(result->description) - 1] = '\0';
                            
                            found = 1;
                            break;
                        }
                    }
                }
            }
            pclose(fp);
            
            if (found) {
                break;
            }
        }
    }
    
    closedir(dir);
    return found;
}

/* 
 * Ищет пакеты с похожими именами
 * Возвращает количество найденных пакетов (до max_results)
 */
int find_similar_packages_by_name_only(const char *pattern, PackageInfo results[], int max_results) {
    DIR *dir;
    struct dirent *entry;
    char *pkglist_dir = "/var/lib/apt/lists";
    int count = 0;
    
    dir = opendir(pkglist_dir);
    if (dir == NULL) {
        return 0;
    }
    
    // Временный массив для всех результатов
    PackageInfo all_results[100];
    int all_count = 0;
    
    // Поиск во всех файлах pkglist
    while ((entry = readdir(dir)) != NULL && all_count < 100) {
        if (strstr(entry->d_name, "pkglist.") != NULL) {
            char filepath[MAX_PATH_LEN];
            snprintf(filepath, sizeof(filepath), "%s/%s", pkglist_dir, entry->d_name);
            
            char cmd[MAX_CMD_LEN];
            int needed = snprintf(cmd, sizeof(cmd), 
                     "pkglist-query \"[%%{FILENAMES}\\t%%{NAME}\\t%%{SUMMARY}\\n]\" \"%s\" 2>/dev/null", 
                     filepath);
            
            if (needed >= (int)sizeof(cmd)) {
                continue;
            }
            
            FILE *fp = popen(cmd, "r");
            if (fp == NULL) {
                continue;
            }
            
            char line[MAX_CMD_LEN];
            while (fgets(line, sizeof(line), fp) != NULL && all_count < 100) {
                char *path = strtok(line, "\t");
                char *package = strtok(NULL, "\t");
                char *description = strtok(NULL, "\n");
                
                if (path != NULL && package != NULL && description != NULL) {
                    // Ищем пакеты, содержащие pattern в имени
                    if (strstr(package, pattern) != NULL) {
                        // Проверяем дубликаты
                        if (!package_already_exists(all_results, all_count, package)) {
                            // Сохраняем информацию о пакете
                            strncpy(all_results[all_count].package_name, package, 
                                   sizeof(all_results[all_count].package_name) - 1);
                            strncpy(all_results[all_count].binary_path, path, 
                                   sizeof(all_results[all_count].binary_path) - 1);
                            strncpy(all_results[all_count].description, description, 
                                   sizeof(all_results[all_count].description) - 1);
                            
                            // Гарантируем завершение строк
                            all_results[all_count].package_name[sizeof(all_results[all_count].package_name) - 1] = '\0';
                            all_results[all_count].binary_path[sizeof(all_results[all_count].binary_path) - 1] = '\0';
                            all_results[all_count].description[sizeof(all_results[all_count].description) - 1] = '\0';
                            
                            all_count++;
                        }
                    }
                }
            }
            pclose(fp);
        }
    }
    
    closedir(dir);
    
    // Сортируем результаты по длине имени
    qsort(all_results, all_count, sizeof(PackageInfo), compare_package_by_name_length);
    
    // Возвращаем не более max_results результатов
    count = (all_count < max_results) ? all_count : max_results;
    for (int i = 0; i < count; i++) {
        memcpy(&results[i], &all_results[i], sizeof(PackageInfo));
    }
    
    return count;
}

/* Выводит справку по использованию программы */
void print_usage() {
    printf("Usage:\n");
    printf("  command-not-found <command>    - Search for a command and suggest packages\n");
    printf("  command-not-found --help       - Show this help message\n");
}

/* Основная функция программы */
int main(int argc, char *argv[]) {
    // Настройка локализации для интернационализации
    setlocale(LC_ALL, "");
    setlocale(LC_MESSAGES, "");

    bindtextdomain("command-not-found", "/usr/share/locale/");
    textdomain("command-not-found");
    
    // Обработка аргументов командной строки
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("command-not-found with direct pkglist query\n");
            return 0;
        }
    }
    
    // Проверка количества аргументов
    if (argc < 2) {
        print_usage();
        return 1;
    }

    // Проверка длины входной команды
    if (strlen(argv[1]) > MAX_INPUT_LEN) {
        fprintf(stderr, "Input command too long\n");
        return 1;
    }

    // Создаем копии оригинальной и преобразованной команды
    char converted_cmd[MAX_INPUT_LEN + 1];
    strncpy(converted_cmd, argv[1], MAX_INPUT_LEN);
    converted_cmd[MAX_INPUT_LEN] = '\0';
    
    char original_cmd[MAX_INPUT_LEN + 1];
    strncpy(original_cmd, argv[1], MAX_INPUT_LEN);
    original_cmd[MAX_INPUT_LEN] = '\0';
    
    // Пытаемся преобразовать команду (автокоррекция раскладки)
    convert_command(converted_cmd);
    if (strcmp(original_cmd, converted_cmd) != 0) {
        // Если команда преобразована и существует - выполняем ее
        if (command_exists_in_path(converted_cmd)) {
            printf("Auto-correcting '%s' to '%s' and executing:\n", original_cmd, converted_cmd);
            execlp(converted_cmd, converted_cmd, NULL);
            perror("exec failed");
            return 1;
        }
    }

    // Пытаемся выполнить оригинальную команду
    if (command_exists_in_path(original_cmd)) {
        execlp(original_cmd, original_cmd, NULL);
        perror("exec failed");
        return 1;
    }

    // Проверяем существование команды в системных каталогах
    if (command_exists_in_system_bin(original_cmd)) {
        printf("%s: %s\n", original_cmd, _("Command found in system directories but not in your PATH"));
        printf("%s 'su - -c \"%s\"'\n", _("Try running as root:"), original_cmd);
        return 127;
    }
    // Проверяем преобразованную команду в системных каталогах
    if (strcmp(original_cmd, converted_cmd) != 0 && command_exists_in_system_bin(converted_cmd)) {
        printf("%s '%s'?\n", _("Did you mean"), converted_cmd);
        printf("%s 'su - -c \"%s\"'\n", _("Try running as root:"), converted_cmd);
        return 127;
    }

    fflush(stdout);

    // Поиск пакета, содержащего команду
    PackageInfo package_info;
    int found = find_package_for_command(converted_cmd, &package_info);

    if (found) {
        if (package_is_installed(package_info.package_name)) {
            // Пакет установлен, но команда не найдена
            printf("%s\n", _("Package is already installed but command not found."));
            printf("%s: %s\n", _("Package"), package_info.package_name);
            printf("%s: %s\n", _("Binary path"), package_info.binary_path);
            printf("%s: %s\n", _("Description"), package_info.description);
        } else {
            // Предлагаем установить пакет
            printf("%s:\n", _("The program can be installed using"));
            printf("su - -c 'apt-get install %s'\n", package_info.package_name);
            printf("%s: %s\n", _("Description"), package_info.description);
        }
    } else {
        // Поиск похожих пакетов по имени
        printf("%s\n", _("Perhaps you were looking for:"));
        
        PackageInfo similar_results[3];
        int result_count = find_similar_packages_by_name_only(converted_cmd, similar_results, 3);
        
        if (result_count > 0) {
            for (int i = 0; i < result_count; i++) {
                if (package_is_installed(similar_results[i].package_name)) {
                    printf("%s [%s]\n", similar_results[i].package_name, _("already installed"));
                } else {
                    printf("%s - %s\n", similar_results[i].package_name, similar_results[i].description);
                }
            }
        } else {
            // Если похожих пакетов не найдено
            printf("%s 'apt-cache search %s'\n", _("Try:"), converted_cmd);
        }
    }

    return 127;
}