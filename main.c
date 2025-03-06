#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include <time.h>

#define MAX_NODES 100
#define MAX_EDGES 10
#define API_URL "https://api.openai.com/v1/chat/completions"
#define BUFFER_SIZE 4096
#define OPENAI_API_KEY "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"

typedef struct {
    char node;
    char neighbors[MAX_NODES];
    int neighbor_count;
} GraphNode;

typedef struct {
    GraphNode nodes[MAX_NODES];
    int node_count;
} Graph;

struct MemoryStruct {
    char* memory;
    size_t size;
};

size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int edge_exists(GraphNode* node, char neighbor) {
    for (int i = 0; i < node->neighbor_count; i++) {
        if (node->neighbors[i] == neighbor) {
            return 1;
        }
    }
    return 0;
}

void add_edge(Graph* graph, char node1, char node2) {
    int index1 = node1 - 'A';
    int index2 = node2 - 'A';

    if (!edge_exists(&graph->nodes[index1], node2)) {
        graph->nodes[index1].neighbors[graph->nodes[index1].neighbor_count++] = node2;
    }
    if (!edge_exists(&graph->nodes[index2], node1)) {
        graph->nodes[index2].neighbors[graph->nodes[index2].neighbor_count++] = node1;
    }
}

void generate_random_graph(Graph* graph, int num_nodes) {
    if (num_nodes > MAX_NODES) num_nodes = MAX_NODES;
    graph->node_count = num_nodes;

    for (int i = 0; i < num_nodes; i++) {
        graph->nodes[i].node = 'A' + i;
        graph->nodes[i].neighbor_count = 0;
    }

    for (int i = 0; i < num_nodes; i++) {
        int edge_count = rand() % (MAX_EDGES + 1);

        for (int j = 0; j < edge_count; j++) {
            char random_neighbor = 'A' + (rand() % num_nodes);

            if (random_neighbor != graph->nodes[i].node) {
                add_edge(graph, graph->nodes[i].node, random_neighbor);
            }
        }
    }
}

void print_graph(Graph* graph) {
    for (int i = 0; i < graph->node_count; i++) {
        printf("%c:", graph->nodes[i].node);
        for (int j = 0; j < graph->nodes[i].neighbor_count; j++) {
            printf(" %c", graph->nodes[i].neighbors[j]);
        }
        printf("\n");
    }
}


void ask_bot(const char* user_input, char* response, size_t response_size) {
    if (!user_input || strlen(user_input) == 0) {
        fprintf(stderr, "\nBlad: Pusta wiadomosc wejsciowa.\n");
        return;
    }

    CURL* curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char full_prompt[BUFFER_SIZE];
    snprintf(full_prompt, sizeof(full_prompt), "%s Na podstawie tego musisz stworzyc graf w formie listy sasiedztwa. Wypisz tylko ta liste. W odpowiedzi nie uzywaj polskich znakow prosze.", user_input);

    json_t* root = json_object();
    json_object_set_new(root, "model", json_string("gpt-4o-mini"));

    json_t* messages = json_array();
    json_t* user_message = json_object();
    json_t* content_json = json_string(full_prompt);

    if (!content_json) {
        fprintf(stderr, "\nBlad: json_string zwrocil NULL.\n");
        json_decref(root);
        return;
    }

    json_object_set_new(user_message, "role", json_string("user"));
    json_object_set_new(user_message, "content", content_json);
    json_array_append_new(messages, user_message);
    json_object_set_new(root, "messages", messages);

    char* post_data = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[BUFFER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", OPENAI_API_KEY);
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_URL, API_URL);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "\nBlad CURL: %s\n", curl_easy_strerror(res));
        }
        else {

            json_t* response_json;
            json_error_t error;
            response_json = json_loads(chunk.memory, 0, &error);
            if (response_json) {
                json_t* choices = json_object_get(response_json, "choices");
                if (json_is_array(choices) && json_array_size(choices) > 0) {
                    json_t* first_choice = json_array_get(choices, 0);
                    json_t* message = json_object_get(first_choice, "message");
                    json_t* content = json_object_get(message, "content");
                    if (json_is_string(content)) {
                        snprintf(response, response_size, "%s", json_string_value(content));
                    }
                    else {
                        fprintf(stderr, "\nBlad: Brak poprawnej odpowiedzi w JSON.\n");
                    }
                }
                json_decref(response_json);
            }
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    free(post_data);
    free(chunk.memory);
    curl_global_cleanup();
}

int main() {
    srand(time(NULL));
    Graph graph;
    int choice, num_nodes;
    char response[BUFFER_SIZE] = "";
    char user_input[BUFFER_SIZE];

    printf("Wybierz tryb:\n1 - Generowanie losowego grafu\n2 - Generowanie grafu z AI\nTwoj wybor: ");
    scanf("%d", &choice);
    getchar();

    if (choice == 1) {
        printf("\nPodaj liczbe wierzcholkow: ");
        scanf("%d", &num_nodes);
        if (num_nodes <= 0 || num_nodes > MAX_NODES) {
            printf("\nNieprawidlowa liczba wierzcholkow.\n");
            return 1;
        }
        generate_random_graph(&graph, num_nodes);
        print_graph(&graph);
    }
    else if (choice == 2) {
        printf("\nWpisz polecenie:\n");
        fgets(user_input, BUFFER_SIZE, stdin);
        user_input[strcspn(user_input, "\n")] = 0;
        ask_bot(user_input, response, BUFFER_SIZE);
        printf("\nOdpowiedz AI:\n\n%s\n", response);
    }
    else {
        printf("\nNieprawidlowy wybor.\n");
        return 1;
    }
    return 0;
}
