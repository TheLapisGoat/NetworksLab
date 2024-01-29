#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 256
#define MAX_SUBJECT_LENGTH 50

char *strstrip(char *s)
{
        size_t size;
        char *end;

        size = strlen(s);

        if (!size)
                return s;

        end = s + size - 1;
        while (end >= s && isspace(*end))
                end--;
        *(end + 1) = '\0';

        while (*s && isspace(*s))
                s++;

        return s;
}

int main() {
    char from[MAX_LINE_LENGTH];
    char to[MAX_LINE_LENGTH];
    char subject[MAX_SUBJECT_LENGTH + 1];  // Additional 1 for null terminator
    char message[MAX_LINE_LENGTH];
    char line[MAX_LINE_LENGTH];

    char test[10];
    fgets(test, sizeof(test), stdin);
    char *test2 = strstrip(test);
    int test3 = atoi(test2);
    printf("%d\n", test3);
    // Read "From:" line
    printf("From: ");
    fgets(from, sizeof(from), stdin);
    if (sscanf(from, "From: %255[^@]@%255[^\n]", line, line) != 2) {
        printf("Invalid 'From' format\n");
        return 1;
    }
    printf("%s\n", line);
    // Read "To:" line
    printf("To: ");
    fgets(to, sizeof(to), stdin);
    if (sscanf(to, "To:%255[^@]@%255[^\n]", line, line) != 2) {
        printf("Invalid 'To' format\n");
        return 1;
    }

    // Read "Subject:" line
    printf("Subject: ");
    fgets(subject, sizeof(subject), stdin);
    // Remove trailing newline
    subject[strcspn(subject, "\n")] = '\0';
    if (strlen(subject) > MAX_SUBJECT_LENGTH) {
        printf("Subject exceeds maximum length\n");
        return 1;
    }

    // Read message body
    printf("Message Body (type '.' on a line by itself to end):\n");
    strcpy(message, "");
    while (1) {
        fgets(line, sizeof(line), stdin);
        // Check for termination condition (line with only a full stop)
        if (strcmp(line, ".\n") == 0) {
            break;
        }
        strcat(message, line);
    }

    // Print the collected information
    printf("\n-----\n");
    printf("From: %s", from);
    printf("To: %s", to);
    printf("Subject: %s\n", subject);
    printf("Message Body:\n%s\n", message);

    return 0;
}
