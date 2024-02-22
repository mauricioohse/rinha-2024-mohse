#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <jansson.h>

int all_files_checked = 0;

int GetGETid(char *uri)
{
    // valid uri is /clientes/x/extrato, where x is 1 to 5
    // anything else is invalid
    if (strncmp(uri, "/clientes/", 10) == 0 && strncmp(uri + 11,"/extrato",8) == 0)
    {
        int x = uri[10] - '0';
        if (x >= 1 && x <= 5)
            return x;

        return -1;
    }

}

char* GoToJSON(char* buffer)
{
    char* jsonstring = strstr(buffer, "{");
    printf("jsonstring:\n %s\n", jsonstring);
    return jsonstring;
}

void Respond404(int clientfd)
{
    char response[256] = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
    printf("we sent back:\n%s\n", response);
    send(clientfd, response, strlen(response), 0);

}

int GetPOSTid(char *uri)
{


    int id;
    if(sscanf(uri, "/clientes/%d/transacoes", &id) < 0 )
        return -1;

    if (id >= 1 && id <= 5)
        return id;

    return -1;

}

void FormatSaldoLine(long long int saldo, long long int limite, char *newSaldoLine)
{
    sprintf(newSaldoLine, "%lld,%lld", limite, saldo);
    for (int i = strlen(newSaldoLine); i < 29; i++)
    {
        newSaldoLine[i] = ' ';
    }
    newSaldoLine[29] = '\n';
}

void appendToResponse(char *response, int id)
{
    const char filename_format[256] = "%d.csv";
    char filename[256];
    sprintf(filename, filename_format, id);
    FILE *file = fopen(filename, "r");

    char line[256];
    fgets(line, 256, file);
    strcat(response, line);

    int n = 0;
    while (fgets(line, 256, file) && n < 10)
    {
        strcat(response, line);
        n++;
    }

    fclose(file);
}

void CheckDBFiles()
{
    const long long int init_limits[] = {100000, 80000, 1000000, 10000000, 500000};
    if (all_files_checked)
        return;

    for (int i = 1; i <= 5; i++)
    {
        const char filename_format[256] = "%d.csv";
        char filename[256];
        sprintf(filename, filename_format, i);
        FILE *file = fopen(filename, "r");

        if (file == NULL)
        {
            file = fopen(filename, "w");
            fprintf(file, "%lld,0                                  \n", init_limits[i - 1]);
            fclose(file);
        }
        else
        {
            fclose(file);
        }
    }

    all_files_checked = 1;
}

int main() {
    CheckDBFiles();

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        0};

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sockfd, (const struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        exit(1);
    }

    listen(sockfd, 10);





    for (;;) {
        int clientfd = accept(sockfd, 0, 0);
        // stdin - 0
        struct pollfd fds[2] = {
            {0,
             POLLIN,
             0},
            {clientfd,
             POLLIN,
             0}};
        char buffer[256] = { 0 };

        poll(fds, 2, 50000);

        if (fds[0].revents & POLLIN) {
            read(0, buffer, 255);
            if(buffer[0] == 'q')
            {
                printf("server turning off\n");
                close(clientfd);
                close(sockfd);
                return 0;
            }
        } else if (fds[1].revents & POLLIN) {
            if (recv(clientfd, buffer, 255, 0) == 0) {
                printf("\nclient disconnected\n");
                close(clientfd); //return 0; // if nothing on the socket, client disconnected so server turns off
            }

            printf("client:\n%s\n", buffer);

            // parse HTTP request
            char method[8], uri[256], version[16];
            sscanf(buffer, "%s %s %s", method, uri, version);

            if (strncmp(method, "GET", 3)==0) {
                // GET request
                printf("client GET request received\n");

                int id = GetGETid(uri);
                if (id == -1)
                {
                    Respond404(clientfd);
                    break;
                }

                // send HTTP response
                char response_body[2000] = {0};
                appendToResponse(response_body, id);

                char response[2256] = {0};
                sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %lud\r\n\r\n%s", strlen(response_body), response_body);

                printf("we sent back:\n%s\n", response);
                send(clientfd, response, strlen(response), 0);

            } else if (strncmp(method, "POST", 4)==0) {
                // POST request
                printf("client POST\n");

                int id = GetPOSTid(uri);

                json_t *root;
                json_error_t error;

                char* jsonstring = GoToJSON(buffer);

                printf("jsonstring:\n%s\n", jsonstring);

                root = json_loads(jsonstring, 0, &error);

                if (!root)
                {
                    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
                    return 1;
                }

                // extracts data from json
                json_t *valor, *tipo, *descricao;

                valor = json_object_get(root, "valor");
                if (!json_is_integer(valor))
                {
                    fprintf(stderr, "error: valor is not an int\n");
                    json_decref(root);
                    return 1;
                }

                tipo = json_object_get(root, "tipo");
                if (!json_is_string(tipo))
                {
                    fprintf(stderr, "error: tipo is not a string\n");
                    json_decref(root);
                    return 1;
                }

                descricao = json_object_get(root, "descricao");
                if (!json_is_string(descricao))
                {
                    fprintf(stderr, "error: descricao is not a string\n");
                    json_decref(root);
                    return 1;
                }

                printf("valor: %lld\n", json_integer_value(valor));
                printf("tipo: %s\n", json_string_value(tipo));
                printf("descricao: %s\n", json_string_value(descricao));
                fflush(stdout);

                //json_decref(root);

                // add data to csv file
                const char filename_format[256] = "%d.csv";
                char filename[256];
                sprintf(filename, filename_format, id);
                FILE *file = fopen(filename, "r+");

                char line[256];
                fgets(line, 256, file);

                long long int limite, saldo;
                sscanf(line, "%lld,%lld", &limite, &saldo);

                if (strcmp(json_string_value(tipo), "d") == 0)
                {
                    saldo -= json_integer_value(valor);
                }
                else if (strcmp(json_string_value(tipo), "c") == 0)
                {
                    saldo += json_integer_value(valor);
                }

                if (saldo < -limite)
                {
                    char response[256] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 18\r\n\r\n422 Limite estorou";
                    send(clientfd, response, strlen(response), 0);
                    fclose(file);
                    continue;
                }

                const char transaction_data_format[256] ="%lld,%s,%s\n";
                char transaction_data[256];
                sprintf(transaction_data, transaction_data_format, json_integer_value(valor), json_string_value(tipo), json_string_value(descricao));

                fseek(file, 0, SEEK_END);
                fprintf(file, "%s", transaction_data);

                rewind(file);
                char newSaldoLine[30] ={0};
                FormatSaldoLine(saldo, limite, newSaldoLine);
                fprintf(file, "%s", newSaldoLine);


                fclose(file);

                char response[256] = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nPOST received!";
                send(clientfd, response, strlen(response), 0);
            }
            else
            {
                printf("unknown request received\n");
            }
            
        }
    }

    return 0;
}
