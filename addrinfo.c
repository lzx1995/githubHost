// Copyright (c) 2024 Zhixiong Li.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "cJSON.h"
#include <sys/stat.h>
#include "hashmap.h"

// Global variable to store the result map
struct web {
    char* site;
    char* ip;
};

int web_compare(const void *a, const void *b, void *udata) {
    const struct web *ua = a;
    const struct web *ub = b;
    return strcmp(ua->site, ub->site);
}

bool web_iter(const void *item, void *udata) {
    const struct web *web = item;
    printf("%s (age=%d)\n", web->site, web->ip);
    return true;
}

uint64_t web_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct web *web = item;
    return hashmap_sip(web->site, strlen(web->site), seed0, seed1);
}

struct hashmap *map = NULL;

// Function to check if a file exists
int fileExists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Function to get the file modification time
time_t getFileModificationTime(const char* filename) {
    struct stat attrib;
    if (stat(filename, &attrib) == 0) {
        return attrib.st_mtime;
    }
    return 0;
}

// Callback function to handle received data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, char* output) {
    size_t totalSize = size * nmemb;
    strncat(output, (char*)contents, totalSize);
    return totalSize;
}

// Function to get hosts
int getHosts() {
    const char* filename = "hosts.json";
    if (!fileExists(filename) || (time(0) - getFileModificationTime(filename)) > 30 * 60) {
        if (map == NULL) {
            map = hashmap_new(sizeof(struct web), 0, 0, 0, 
                                     web_hash, web_compare, NULL, NULL);
        }
        const char* url = "https://raw.hellogithub.com/hosts.json";
        CURL* curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "Error initializing libcurl!\n");
            return 1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);

        char responseData[8192]; // Adjust the size as needed

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, responseData);
        printf("download hosts....\n");
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 1;
        }

        curl_easy_cleanup(curl);

        FILE* outputFile = fopen(filename, "w");
        if (outputFile) {
            fputs(responseData, outputFile);
            fclose(outputFile);
        } else {
            fprintf(stderr, "Error opening file for writing.\n");
        }
        hashmap_clear(map, false);
        cJSON* root = cJSON_Parse(responseData);
        if (root == NULL) {
            fprintf(stderr, "Error parsing JSON data.\n");
            return 1;
        }

        cJSON* entry = root->child;
        while (entry != NULL) {
            if (entry->type == cJSON_Array && cJSON_GetArraySize(entry) == 2) {
                cJSON* ip = cJSON_GetArrayItem(entry, 0);
                cJSON* host = cJSON_GetArrayItem(entry, 1);

                if (ip->type == cJSON_String && host->type == cJSON_String) {
                    hashmap_set(map, &(struct web){ .site=strdup(host->valuestring), .ip=strdup(ip->valuestring) });
                }
            }

            entry = entry->next;
        }

        cJSON_Delete(root);

        return 0;
    } else {
        if (map == NULL) {
            map = hashmap_new(sizeof(struct web), 0, 0, 0, 
                                     web_hash, web_compare, NULL, NULL);
            FILE* inputFile = fopen(filename, "r");
            printf("open hosts file\n");
            if (inputFile) {
                fseek(inputFile, 0, SEEK_END);
                long fileSize = ftell(inputFile);
                fseek(inputFile, 0, SEEK_SET);

                char* fileContent = (char*)malloc(fileSize + 1);
                if (fileContent) {
                    fread(fileContent, 1, fileSize, inputFile);
                    fileContent[fileSize] = '\0';  // Null-terminate the string

                    cJSON* root = cJSON_Parse(fileContent);
                    if (root == NULL) {
                        fprintf(stderr, "Error parsing JSON data.\n");
                        free(fileContent);
                        fclose(inputFile);
                        return 1;
                    }

                    cJSON* entry = root->child;
                    while (entry != NULL) {
                        if (entry->type == cJSON_Array && cJSON_GetArraySize(entry) == 2) {
                            cJSON* ip = cJSON_GetArrayItem(entry, 0);
                            cJSON* host = cJSON_GetArrayItem(entry, 1);

                            if (ip->type == cJSON_String && host->type == cJSON_String) {
                                hashmap_set(map, &(struct web){ .site=strdup(host->valuestring), .ip=strdup(ip->valuestring) });
                            }
                        }

                        entry = entry->next;
                    }

                    cJSON_Delete(root);
                    free(fileContent);
                    fclose(inputFile);
                } else {
                    fprintf(stderr, "Error allocating memory for file content.\n");
                }
            } else {
                fprintf(stderr, "Error opening file for reading.\n");
            }
        }
    }

    return 0;
}


int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    // Get the original getaddrinfo function
    int (*original_getaddrinfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
    original_getaddrinfo = dlsym(RTLD_NEXT, "getaddrinfo");

    // Replace node with a different domain if needed
    // Call the original getaddrinfo function
    int ret = original_getaddrinfo(node, service, hints, res);
    if (strcmp(node, "raw.hellogithub.com") == 0) { // break recursive
        return ret;
    }
    getHosts();
    struct web *cmpweb = hashmap_get(map, &(struct web){ .site= node });
    if (cmpweb) {
        struct addrinfo *p;
        char ipstr[INET6_ADDRSTRLEN];
        for (p = *res; p != NULL; p = p->ai_next) {
            void *addr;
            char *ipver;

            // 获取指向地址的指针
            if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                inet_pton(AF_INET, cmpweb->ip, &(ipv4->sin_addr));
                addr = &(ipv4->sin_addr);
                ipver = "IPv4";
            } else { // IPv6
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipver = "IPv6";
            }
        }
        return ret;
    } else {
        return ret;
    }
    
}