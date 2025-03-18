#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// jākompilē ar -lm
#include <math.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>

// izdalītās atmiņas (maksimālais) kopējais apjoms ir 1024 baiti
// piemēram, mem-frag-tests-1/chunks2.txt kopējais chunk'u izmērs ir 1023
// bet tad nepietiek atmiņas dienesta informācijai
// tātad bufera izmēram jābūt lielākam
#define TOTAL_MEMORY 2048
unsigned char buffer[TOTAL_MEMORY];

// dienesta informācija (metadati)
typedef struct chunk_header
{
    struct chunk_header *next;
    unsigned int size : 24; // 3 baiti
    unsigned int free : 1; // 1 bits
} __attribute__((packed, aligned(4))) chunk_header; // ietaupa 4 baitus, bet tādēļ var būt lēnāka ātrdarbība...

static chunk_header *buffer_start = NULL;

// void* nozīmē "generic pointer", var atgriezt NULL vai pointeri uz jebko
// atgriežam pointeri uz izdalīto atmiņu, vai NULL, ja neizdevās rezervēšana
void *best_fit(chunk_header *current, int insert)
{
    unsigned int closest_fit_number;
    chunk_header *closest_fit = NULL;
    closest_fit_number = -1;

    while (current != NULL) {
        if (current->size - insert < 0) {
            current = current->next;
            continue;
        }

        if (current->size == insert) {
            closest_fit = current;
            break;
        } else {
            if (closest_fit_number == -1) {
                closest_fit_number = current->size - insert;
                closest_fit = current;
            } else {
                if (current->size - insert < closest_fit_number) {
                    closest_fit_number = current->size - insert;
                    closest_fit = current;
                }
            }
            current = current->next;
        }
    }
    return closest_fit;
}

void *worst_fit(size_t size)
{

}

void *first_fit(size_t size)
{

}

void *next_fit(size_t size)
{

}

// lasa skaitļus (chunks vai sizes) no chunks faila vai sizes faila
int *read_nums_from_file(const char *filename, int *count)
{
    FILE *f = fopen(filename, "r");
    if(!f) {
        fprintf(stderr, "Kļūda, atvērot failu %s: %m\n", filename);
        exit(1);
    }

    int capacity = 15;
    int *nums = malloc(capacity * sizeof(int));

    int n = 0;
    int num;
    while(fscanf(f, "%d", &num) == 1) {
        if(n >= capacity) {
            capacity *= 2;
            nums = realloc(nums, capacity * sizeof(int));
        }
        nums[n++] = num;
    }
    
    if(n < 1) {
        fprintf(stderr, "Kļūda: fails %s ir tukšs vai nederīgs\n", filename);
        free(nums);
        exit(1);
    }

    fclose(f);
    *count = n;
    return nums;
}

// inicializē buferi (kopējo atmiņu) tā, kā aprakstīts chunks failā
void init_buffer(const char *chunks_file)
{
    int count = 0;
    int *chunks = read_nums_from_file(chunks_file, &count);

    size_t total_used = 0;
    size_t total_chunks = 0;
    for(int i = 0; i < count; i++) {
        total_used += chunks[i] + sizeof(chunk_header);
        total_chunks += chunks[i];
    }

    if(total_used > TOTAL_MEMORY) {
        fprintf(stderr, "Kļūda: norādīto chunku kopējais izmērs (%zu) pārsniedz %d baitus. Chunku izmērs bez chunk_header ir %zu\n", total_used, TOTAL_MEMORY, total_chunks);
        free(chunks);
        exit(1);
    }

    // inicializē pirmo chunk (head)
    unsigned char *current =  buffer;
    chunk_header *head = (chunk_header *)current;
    head->free = 1;
    head->size = chunks[0];
    current += sizeof(chunk_header) + head->size;
    chunk_header *prev = head;

    // inicializē parējos chunk
    for(int i = 1; i < count; i++) {
        chunk_header *chunk = (chunk_header *)current;
        chunk->free = 1;
        chunk->size = chunks[i];
        prev->next = chunk;
        prev = chunk;
        current += sizeof(chunk_header) + chunk->size;
    }
    prev->next = NULL;

    free(chunks);

    buffer_start = head;
}

void print_buffer()
{
    chunk_header *curr = buffer_start;
    int i = 0;
    printf("Atmiņas stāvoklis:\n");
    while(curr) {
        printf(" Bloks %d: addr=%p, izmērs=%d, brīvs=%d, next=%p\n",
               i, (void *)curr, curr->size, curr->free, (void *)curr->next);
        curr = curr->next;
        i++;
    }
}

void show_usage()
{
    printf("Izmantošana: md5 -c chunks -s sizes\n");
}

// testēšanas funkcija, pagaidām izveidota tā, ka atkomentē, lai pārbaudītu noteiktu algoritmu
void test(char *chunks_file, char *sizes_file)
{
    int count = 0;
    int unassigned = 0;
    int *insert = read_nums_from_file(sizes_file, &count);
    init_buffer(chunks_file);
    chunk_header *current = buffer_start;
    chunk_header *assign = NULL;
    clock_t start = clock(), end;

    for (int i = 0; i < count; i++) {
        assign = best_fit(current, insert[i]);
        // assign = first_fit(current, insert[i]);
        // assign = worst_fit(current, insert[i]);
        // assign = next_fit(current, insert[i]);
        if (assign == NULL) {
            unassigned += insert[i];
        } else {
            assign->size = assign->size - insert[i];

            if (assign->size == 0) {
                assign->free = 0;
            }
        }
    }
    end = clock();
    free(insert);

    printf("Nepiešķirtās atmiņas daudzums: %d\n", unassigned);

    if (unassigned != 0) {
        chunk_header *it = buffer_start;
        double max_free = 0;
        double free_total = 0;
        double quality = 0;
        while (it != NULL) {
            if (it->size > max_free) {
                max_free = it->size;
            }
            quality += it->size * it->size;
            free_total += it->size;
            it = it->next;
        }

        // no https://en.wikipedia.org/wiki/Fragmentation_(computing)#Comparison
        printf("Fragmentācija (1.versija) = %f%%\n", (1-(max_free/free_total))*100);
        // no https://asawicki.info/news_1757_a_metric_for_memory_fragmentation
        printf("Fragmentācija (2.versija) = %f%%\n", (1-pow(sqrt(quality)/free_total, 2))*100);
    } else {
        printf("Fragmentācija = 0%%\n");
    }

    printf("Kopējais laiks sekundēs: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
    print_buffer();
}

int main(int argc, char **argv)
{
    char *chunks_file = NULL;
    char *sizes_file = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-c") == 0) {
            // jāparbauda, vai seko faila nosaukums
            if(i + 1 >= argc) {
                show_usage();
                return 1;
            }
            chunks_file = argv[++i];
        } else if(strcmp(argv[i], "-s") == 0) {
            if(i + 1 >= argc) {
                show_usage();
                return 1;
            }
            sizes_file = argv[++i];
        }
    }

    if(!chunks_file || !sizes_file) {
        show_usage();
        exit(1);
    }

    test(chunks_file, sizes_file);

    return 0;
}