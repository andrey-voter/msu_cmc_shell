#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>

char **get_matrix(char *str);

int execute_cmd(char **matrix, int fd1, int fd2);

int get_str(char *str) {
    char c;
    int i = 0;
    while ((c = getchar()) != '\n')
        str[i++] = c;
    str[i] = '\0';
    return 0;
}

int get_simplecmd(char *str, int fd_in, int fd_out);

int get_conv(char *cmd, int flag);

int get_cmd(char *cond_cmd);

int get_cond_cmd(
        char *str)   // по исходной строчке в cmd делает команду с условным выполнением  (парсим до & или до ;)
{
    int k1 = 0;
    if (str == NULL) return 1;
    size_t start = 0;
    size_t end = 0;
    int ex_code = 0;

    while (str[end] != '\0' && str[end] != '\n') {
        while (str[end] != ';' && str[end] != '&' && str[end] != '\0' && str[end] != '\n' ||
               k1 != 0) {
            if (str[end] == '(')
                k1++;
            if (str[end] == ')')
                k1--;
            end++;
        }

        if (str[end] == ';') {
            str[end] = '\0';
            end++;
            ex_code = get_cmd(str + start);
            int n = ARG_MAX;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        } else if (str[end] == '&' && str[end + 1] != '&') {
            pid_t p1, p2;
            p1 = fork();
            if (!p1) // сын запускает внука и умирает
            {
                p2 = fork();
                if (p2) // сын
                {
                    exit(0);
                }
                if (!p2) // внук работает в фоне
                {
                    signal(SIGINT, SIG_IGN);
                    int fd = open("/dev/null", O_RDWR);
                    dup2(fd, 0);
                    str[end] = '\0';
                    end++;

                    ex_code = get_cmd(str + start);
                    exit(ex_code);
                }
            }
            wait(NULL);
            end++;
            int n = ARG_MAX;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        } else if (str[end] == '&' && str[end + 1] == '&')
            end += 2;
        else if (str[end] == '\0' || str[end] == '\n') {
            ex_code = get_cmd(str + start);
            int n = ARG_MAX;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        }
    }
    return ex_code;
}

int
get_cmd(char *str) // по команде с условным выполнением делает команду (парсим до && или до ||)
{
    int k1 = 0;
    if (str == NULL) return 1;
    size_t start = 0;
    size_t end = 0;
    int flag = 1;
    int flag2 = 1;
    int ex_code = 0;
    while (str[end] != '\0' && str[end] != '\n') {
        while (str[end] != '&' && str[end] != '|' && str[end] != '\0' && str[end] != '\n' ||
               k1 != 0) {
            if (str[end] == '(')
                k1++;
            if (str[end] == ')')
                k1--;
            end++;
        }
        if ((str[end] == '&' && str[end + 1] == '&') || (str[end] == '|' && str[end + 1] == '|')) {
            if (str[end] == '&') flag2 = 1; else flag2 = 0;
            str[end] = '\0';
            end += 2;
            if (flag == 1 && ex_code == 0 || flag == 0 && ex_code != 0)
                ex_code = get_conv(str + start, flag);
            flag = flag2;
            int n = ARG_MAX;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        } else if (str[end] != '\0' && str[end] != '\n') end++;
    }
    if (str[end] == '\0' || str[end] == '\n') {
        //if (str[end-1] == '&' && str[end-2] == '&') flag = 1; else flag = 0;
        if (flag == 1 && ex_code == 0 || flag == 0 && ex_code != 0)
            ex_code = get_conv(str + start, flag);
    }
    return ex_code;
}

int get_conv(char *str, int flag) // по команде получаем конвейер
{
    int fd1 = 0;
    int fd2 = 1;
    if (str == NULL) return 1;
    int j;
    int is_begin_in = 0;
    int is_begin_out = 0;
    int f_in = 0;
    int f_out = 0;
    size_t start = 0;
    size_t end = 0;
    int n = ARG_MAX;
    int ex_code = 0;
    while (n--) {
        if (str[end] == ' ') end++;
        else break;
    }
    if (str[end] == '(') {
        while (str[end] != ')' && str[end] != '\0')
            end++;
        str[end] = '\0';
        pid_t p = fork();
        if (!p) {
            get_cond_cmd(str + 1);
            exit(0);
        }
        waitpid(p, &ex_code, 0);
        return ex_code;
    }

    while (str[end] != '\0' && str[end] != '\n') {
        if (str[end] == '<') f_in = 1;
        if (str[end] == '>') f_out = 1;
        end++;
    }
    end = 0;
    n = ARG_MAX;
    while (n--)   // если в начале были пробелы, то мы их сжали
    {
        if (str[end] == ' ') end++;
        else break;
    }
    if (str[end] == '<') is_begin_in = 1;
    if (str[end] == '>') is_begin_out = 1;
    end = 0;
    if (f_in && !f_out && is_begin_in) // есть ввод, нет вывода, ввод в начале
    {
        end = 0;
        while (str[end] != '<')
            end++;
        n = ARG_MAX;
        end++;
        while (n--) {
            if (str[end] == ' ') end++;
            else break;
        }
        char f_in_name[ARG_MAX];
        int j = 0;
        while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
               str[end] != '\n')
            f_in_name[j++] = str[end++];
        f_in_name[j] = '\0';
        fd1 = open(f_in_name, O_RDONLY);
        printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
        end++;
        start = end;
        ex_code = get_simplecmd(str + start, fd1, fd2);
        return ex_code;
    }
    if (f_in && !f_out && !is_begin_in) // есть ввод, нет вывода, ввод в конце
    {
        end = 0;
        while (str[end] != '<')
            end++;
        n = ARG_MAX;
        int init = end;
        end++;
        while (n--) {
            if (str[end] == ' ') end++;
            else break;
        }
        char f_in_name[ARG_MAX];
        j = 0;
        while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
               str[end] != '\n')
            f_in_name[j++] = str[end++];
        f_in_name[j] = '\0';
        fd1 = open(f_in_name, O_RDONLY);
        printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
        str[init] = '\0';
        ex_code = get_simplecmd(str + start, fd1, fd2);
        return ex_code;
    }
    if (f_out && !f_in && is_begin_out) // есть вывод, нет ввода, вывод в начале
    {
        end = 0;
        while (str[end] != '>')
            end++;
        int append = (str[end + 1] == '>');
        if (append) end++;
        n = ARG_MAX;
        end++;
        while (n--) {
            if (str[end] == ' ') end++;
            else break;
        }
        char f_out_name[ARG_MAX];
        int j = 0;
        while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
               str[end] != '\n')
            f_out_name[j++] = str[end++];
        f_out_name[j] = '\0';
        if (!append) {
            fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        } else {
            fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
        }
        printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
        end++;
        start = end;
        ex_code = get_simplecmd(str + start, fd1, fd2);
        return ex_code;
    }
    if (f_out && !f_in && !is_begin_out) // есть вывод, нет ввода, вывод в конце
    {
        end = 0;
        while (str[end] != '>')
            end++;
        int append = (str[end + 1] == '>');
        int init = end;
        if (append) end++;
        n = ARG_MAX;
        end++;
        while (n--) {
            if (str[end] == ' ') end++;
            else break;
        }
        char f_out_name[ARG_MAX];
        j = 0;
        while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
               str[end] != '\n')
            f_out_name[j++] = str[end++];
        f_out_name[j] = '\0';
        if (!append) {
            fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        } else {
            fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
        }
        printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
        str[init] = '\0';
        ex_code = get_simplecmd(str + start, fd1, fd2);
        return ex_code;
    }
    if (f_in && f_out && (is_begin_in || is_begin_out)) // есть и ввод и вывод, в начале
    {
        int first_in = 0;
        int first_out = 0;
        end = 0;
        while (str[end] != '>' && str[end] != '<')
            end++;
        if (str[end] == '>')
            first_out = 1;
        else first_in = 1;
        if (first_out) // сначала вывод
        {
            int append = (str[end + 1] == '>');
            if (append) end++;
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_out_name[ARG_MAX];
            int j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_out_name[j++] = str[end++];
            f_out_name[j] = '\0';
            if (!append) {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            } else {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            end++;
            char f_in_name[ARG_MAX];
            j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_in_name[j++] = str[end++];
            f_in_name[j] = '\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            end++;
            start = end;
            ex_code = get_simplecmd(str + start, fd1, fd2);
            return ex_code;
        }
        if (first_in) // сначала ввод
        {
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_in_name[ARG_MAX];
            int j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_in_name[j++] = str[end++];
            f_in_name[j] = '\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_out_name[ARG_MAX];
            j = 0;
            int append = (str[end + 1] == '>');
            if (append) end++;
            end++;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_out_name[j++] = str[end++];
            f_out_name[j] = '\0';
            if (!append) {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            } else {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            end++;
            start = end;
            ex_code = get_simplecmd(str + start, fd1, fd2);
            return ex_code;
        }
    }
    if (f_in && f_out && (!is_begin_in && !is_begin_out)) // есть и ввод и вывод, в конце
    {
        int first_in = 0;
        int first_out = 0;
        end = 0;
        while (str[end] != '>' && str[end] != '<')
            end++;
        if (str[end] == '>')
            first_out = 1;
        else first_in = 1;
        int init = end;
        if (first_out) // сначала вывод
        {
            int append = (str[end + 1] == '>');
            if (append) end++;

            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_out_name[ARG_MAX];
            int j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_out_name[j++] = str[end++];
            f_out_name[j] = '\0';
            if (!append) {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            } else {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            n = ARG_MAX;
            end++;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_in_name[ARG_MAX];
            j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_in_name[j++] = str[end++];
            f_in_name[j] = '\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            end++;
            str[init] = '\0';
            ex_code = get_simplecmd(str + start, fd1, fd2);
            return ex_code;
        }
        if (first_in) // сначала ввод
        {
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            char f_in_name[ARG_MAX];
            int j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_in_name[j++] = str[end++];
            f_in_name[j] = '\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            n = ARG_MAX;
            end++;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            int append = (str[end + 1] == '>');
            if (append) end++;
            end++;
            char f_out_name[ARG_MAX];
            j = 0;
            while (str[end] != ' ' && str[end] != '&' && str[end] != '|' && str[end] != '\0' &&
                   str[end] != '\n')
                f_out_name[j++] = str[end++];
            f_out_name[j] = '\0';
            if (!append) {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            } else {
                fd2 = open(f_out_name, O_WRONLY | O_CREAT | O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            end++;
            str[init] = '\0';
            ex_code = get_simplecmd(str + start, fd1, fd2);
            return ex_code;
        }
    }
    ex_code = get_simplecmd(str, fd1, fd2);  // перенаправления нет вообще
    return ex_code;
}

int get_simplecmd(char *str, int fd1,
                  int fd2) //по конвейеру получаем простую команду, которую и будем загружать в exec
{
    int cnt = 1;
    int status = 0;
    int temp = 0;
    pid_t p;
    if (str == NULL) return 1;
    char string[ARG_MAX];
    strcpy(string, str);
    size_t start = 0;
    size_t end = 0;
    int fl = 0;
    while (string[end] != '\0' && string[end] != '\n') {
        if (string[end] == '|') cnt++;
        end++;
    }
    end = 0;
    if (cnt == 1) {
        while (string[end] != '\0' && string[end] != '\n')
            end++;
        string[end] = '\0';
        char **matrix = get_matrix(string + start);
        p = fork();
        if (!p)
            execute_cmd(matrix, fd1, fd2);
        wait(&status);
        return status;
    }

    int fdd0 = fd1; //ввод в текущую команду
    int fdd1; // вывод из текущей команды
    int fd[2];
    for (int i = 1; i < cnt;) {
        end++;
        if (string[end] == '|') {
            string[end] = '\0';
            char **matrix = get_matrix(string + start);
            end++;
            int n = ARG_MAX;
            while (n--) {
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
            pipe(fd); // канал между текущей и следующей командой
            fdd1 = fd[1]; // вывод из текущей направляем в канал
            p = fork();
            if (!p)
                execute_cmd(matrix, fdd0, fdd1);
            if (fdd0 != 0)  // закрываем на чтение канал у "текущей команды"
                close(fdd0);
            close(fdd1);
            fdd0 = fd[0]; // ввод следующей перенаправляем на канал
            i++;
        }
    }
    while (string[end] != '\0' && string[end] != '\n')
        end++;
    string[end] = '\0';
    char **matrix = get_matrix(string + start);
    fdd1 = fd2; // вывод последней перенаправляем
    p = fork();
    if (!p)
        execute_cmd(matrix, fdd0, fdd1);
    close(fdd0);
    if (fdd1 != 1)
        close(fdd1);
    for (int i = 0; i < cnt; i++) // ждем всех сыновей
    {
        wait(&temp);
        if (status == 0)
            status = temp;
    }
    return status;
}

int execute_cmd(char **matrix, int fd1,
                int fd2) // выполняем команду, учитывая перенаправление ввода-вывода
{
    if (fd1 != 0) dup2(fd1, 0);
    if (fd2 != 1) dup2(fd2, 1);
    execvp(matrix[0], matrix);
    exit(1);
}

char **get_matrix(char *str) // по строке получаем массив строк
{
    int start = 0;
    int end = 0;
    int cnt = 1;
    while (str[end] != '\n' && str[end] != '\0') {
        if (str[end] == ' ' && str[end + 1] != ' ' && str[end + 1] != '\0')
            cnt++;
        end++;
    }
    end = 0;
    char **matrix = (char **) malloc(cnt * sizeof(char *));
    for (int i = 0; i < cnt; i++) {
        matrix[i] = (char *) malloc(1000 * sizeof(char));
    }
    int i = 0;
    while (str[end] != '\n' && str[end] != '\0') {
        while (str[end] != ' ' && str[end] != '\n' && str[end] != '\0')
            end++;
        if (str[end] == '\0') {
            strcpy(matrix[i++], str + start);
            break;
        }
        str[end] = '\0';
        end++;
        strcpy(matrix[i++], str + start);
        start = end;
    }
    matrix[i] = NULL;
    return matrix;
}


int main() {
    while (1) {
        char str[ARG_MAX];
        if (!strcmp(str, "exit")) return 0;
        get_str(str);
        get_cond_cmd(str);
    }
}
