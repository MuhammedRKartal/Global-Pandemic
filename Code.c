#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <semaphore.h>  /* Semaphore */
#include <string.h>     /* String */
#include <fcntl.h>      /* O_CREAT */

/* types */
#define EMPLOYEE 1 // employee type
#define RESIDENT 2 // resident type

#define STORE_COUNT 3               /* number of stores in the neighbourhood */
#define RESIDENT_COUNT 8            /* number of residents in the neighbourhood */
#define EMPLOYEE_COUNT 1            /* number of employees in the store */
#define MAX_PEOPLE_IN_STORE 3       /* max number of people in same store */
#define MAX_FROM_SAME_ITEM 20       /* max number of items that could be bought from the same product */
#define MAX_NUMBER_OF_CATEGORY 3    /* max number of categories in a shopping list */
#define RESIDENT_SLEEP_1 7         /* max time to sleep for resident after buying all of the products */
#define RESIDENT_SLEEP_2 3         /* max time to sleep for resident if some items left in shopping list */
#define EMPLOYEE_SLEEP 3           /* max time to sleep for employe */
#define NUMBER_OF_LISTS 2           /* number of lists to be completed */


struct productCategory {
    int maxStock;
    int currentStock;
    char *name;
};

struct store {
    int currentVisitors;
    char *name;
    struct productCategory *products;
    int totalCategories;
};

struct shoppingListItem {
    char *name;
    int amount;
    int currentlyBoughtAmount;
};

struct shoppingList {
    struct shoppingListItem *items;
    int totalCategories;
};

struct person {
    char *name;
    int visitedStores[STORE_COUNT];
    int role;
    int completedLists;
    struct shoppingList *shoppingList;
};

pthread_t residents[RESIDENT_COUNT];            /* storing pthreads of residents in an array */
pthread_t employees[EMPLOYEE_COUNT];            /* storing pthreads of employees in an array */

pthread_mutex_t items_mutex[STORE_COUNT][3];    /* storing mutex locks of each product in a 2D array */
sem_t *store1_semaphore;
sem_t *store2_semaphore;
sem_t *store3_semaphore;

int t = 0; // t value
int currentResidentCount = RESIDENT_COUNT; //to terminate employee after all of the residents threads are finished

/* store products initialization */
char products[7][100] = {"beverages", "breads", "diary products", "snacks", "frozen foods", "personal care products",
                         "cleaning products"};
struct productCategory store1[3] = {(struct productCategory) {40, 40, "beverages"},
                                    (struct productCategory) {20, 20, "breads"},
                                    (struct productCategory) {50, 50, "diary products"}};
struct productCategory store2[3] = {(struct productCategory) {15, 15, "snacks"},
                                    (struct productCategory) {35, 35, "frozen foods"},
                                    (struct productCategory) {25, 25, "beverages"}};
struct productCategory store3[3] = {(struct productCategory) {10, 10, "personal care products"},
                                    (struct productCategory) {60, 60, "cleaning products"},
                                    (struct productCategory) {20, 20, "beverages"}};

struct store *storeStructs[STORE_COUNT]; // all of the stores array

/* this function allocates a new memory to create a person
and returns a pointer which points to that new person */
struct person *createPerson(char *name, int role) {
    struct person *data = (struct person *) malloc(sizeof(struct person));
    data->name = strdup(name);
    data->role = role;
    return data;
}

/* this function allocates a new memory to create a store
and returns a pointer which points to that new store */
struct store *createStore(char *name, int productsIndex) {
    struct store *data = (struct store *) malloc(sizeof(struct store));
    data->name = strdup(name);
    data->currentVisitors = 0;
    if (productsIndex == 0) data->products = store1;
    if (productsIndex == 1) data->products = store2;
    if (productsIndex == 2) data->products = store3;
    data->totalCategories = 3;
    return data;
}

/* this function is for preventing choosing same category for the same shopping list */
int isExistingInShoppingList(char *product, struct shoppingListItem *wishList, int currentIndex) {
    for (int i = 0; i < currentIndex; i++) {
        if (strcmp((wishList + i)->name, product) == 0) {
            return 1;
        }
    }
    return 0;
}

/* this function allocates a new memory to create a shopping list
and returns a pointer which points to that new shopping list */
struct shoppingList *createShoppingList() {
    struct shoppingList *shoppingList = (struct shoppingList *) malloc(sizeof(struct shoppingList));
    int categoryCount = 1 + (rand() % MAX_NUMBER_OF_CATEGORY);
    shoppingList->totalCategories = categoryCount;
    shoppingList->items = (struct shoppingListItem *) malloc(sizeof(struct shoppingListItem) * categoryCount);
    int i, productCount;
    struct shoppingListItem *itemWishes = shoppingList->items;

    int size = sizeof products / sizeof products[0]; //7

    for (i = 1; i <= categoryCount; i++) {
        productCount = 1 + (rand() % MAX_FROM_SAME_ITEM); // how many items do residents want to buy
        int productIndex = rand() % size; // random item to buy
        while (isExistingInShoppingList(products[productIndex], itemWishes,
                                        i - 1)) // check if it is existing in the list
        {
            productIndex = rand() % size;
        }
        struct shoppingListItem s;
        s.name = malloc(100);
        s.amount = productCount;
        s.currentlyBoughtAmount = 0;
        strcpy(s.name, products[productIndex]);
        *(shoppingList->items + i - 1) = s;
    }
    return shoppingList;
}


void *runner(void *ptr); /* the thread */
void initResidents();

void initEmployees();

void initStores();

int main() {
    initStores();
    initEmployees();
    initResidents();
    sleep(1000);
    return 0;
}

/* this function initializes stores and mutex locks */
void initStores() {
    int i;
    for (i = 0; i < STORE_COUNT; i++) {
        char name[12];
        snprintf(name, 12, "Store %d", i + 1);
        storeStructs[i] = createStore(name, i);


        pthread_mutex_init(&items_mutex[i][0], NULL); // mutex lock initialization for item 0 in store at index i
        pthread_mutex_init(&items_mutex[i][1], NULL); // mutex lock initialization for item 1 in store at index i
        pthread_mutex_init(&items_mutex[i][2], NULL); // mutex lock initialization for item 2 in store at index i
    }
    char randName[12];
    snprintf(randName, 12, "store1%d", rand() % 101);
    store1_semaphore = sem_open(randName, O_CREAT, 0644, MAX_PEOPLE_IN_STORE); // creating semaphore and storing
    snprintf(randName, 12, "store2%d", rand() % 101);
    store2_semaphore = sem_open(randName, O_CREAT, 0644, MAX_PEOPLE_IN_STORE); // creating semaphore and storing
    snprintf(randName, 12, "store3%d", rand() % 101);
    store3_semaphore = sem_open(randName, O_CREAT, 0644, MAX_PEOPLE_IN_STORE); // creating semaphore and storing
}

/* this function initializes residents and their threads */
void initResidents() {
    int i;
    int thread_counter = 0;
    for (i = 1; i <= RESIDENT_COUNT; i++) {
        char name[12];
        snprintf(name, 12, "Resident %d", i);
        struct person *person = createPerson(name, RESIDENT);
        person->shoppingList = createShoppingList();
        pthread_create(&residents[thread_counter], NULL, runner, person);
        thread_counter++;
    }
}

/* this function initializes employees and their threads */
void initEmployees() {
    int i;
    int thread_counter = 0;
    for (i = 1; i <= EMPLOYEE_COUNT; i++) {
        char name[12];
        snprintf(name, 12, "Employee %d", i);
        struct person *employee = createPerson(name, EMPLOYEE);
        pthread_create(&employees[thread_counter], NULL, runner, employee);
        thread_counter++;
    }
}

/*
    if product doesnt exist in store, returns -1
    else, returns index of the product
*/
int storeHasProduct(int storeIndex, char *product, struct person *person) {
    struct store *store = storeStructs[storeIndex];
    struct productCategory *products = store->products;
    struct shoppingListItem *shoppingListItem = person->shoppingList->items;

    for (int i = 0; i < store->totalCategories; i++) {
        if (strcmp((products + i)->name, product) == 0) {
            if ((products + i)->currentStock > 0) {
                return i;
            }
        }
    }
    return -1;
}

/* this function prints info after buying products or filling the products.*/
void printAfterBuyingOrFilling(struct person *person, struct store *store, struct shoppingListItem *previous,
                               struct shoppingListItem *current) {
    char *output = malloc(1000);
    char *temp = malloc(100);
    snprintf(temp, 15 + (sizeof(t) / sizeof(int)), "At time t=%d, ", t);
    strcat(output, temp);


    if (person->role == RESIDENT) {
        snprintf(temp, 17 + sizeof(person->name), "%s buys", person->name);
        strcat(output, temp);

        int countPrinted = 0;

        int flag = 1;
        for (int i = 0; i < person->shoppingList->totalCategories; i++) {
            if ((previous + i)->currentlyBoughtAmount != (current + i)->currentlyBoughtAmount) {
                flag = 0;
                int amount = (current + i)->currentlyBoughtAmount - (previous + i)->currentlyBoughtAmount;
                if (countPrinted > 0) {
                    strcat(output, ",");
                }
                countPrinted++;
                snprintf(temp, 5 + (sizeof(t) / sizeof(int)), " %d ", amount);
                strcat(output, temp);
                strcat(output, (current + i)->name);
            }
        }
        snprintf(temp, 15 + sizeof(store->name), " from %s.\n", store->name);
        strcat(output, temp);
        if (flag == 1) {
            free(output);
            free(temp);
            return;
        }
    } else {
        snprintf(temp, 30 + sizeof(person->name), "%s fills all counters at ", person->name);
        strcat(output, temp);
        snprintf(temp, 10 + sizeof(store->name), "%s.\n", store->name);
        strcat(output, temp);
    }
    printf("%s", output);
    free(output);
    free(temp);
}

/* This function is about to buy items, compare a person's shopping list with current shop, if list items are available go for first available item,
    lock this item with a mutex lock to don't let anyone else buy it, after that buy available and needed items. */
void *buyItems(struct person *person, int storeIndex) {
    /* duplicate previous shopping list item state to print bought items afterwards */
    struct shoppingList *shoppingList = person->shoppingList; //shopping list of current person
    struct shoppingListItem *currentShoppingListItems = shoppingList->items; //items in the shopping list of the current person
    int size = sizeof(struct shoppingListItem) * shoppingList->totalCategories; // memory allocation size
    struct shoppingListItem *previousShoppingListItems = malloc(size);
    memcpy(previousShoppingListItems, currentShoppingListItems, size); // duplicate currentlyBought

    struct productCategory *products = storeStructs[storeIndex]->products; //All products in the specific store

    //runs through all of the item categories in the current customer's shopping list
    for (int i = 0; i < shoppingList->totalCategories; i++) {
        //check if that store has the products if it doesn't have it will make productIndex's value -1
        int productIndex = storeHasProduct(storeIndex, (currentShoppingListItems + i)->name, person);
        //If still there are items in the list and its available
        if (productIndex != -1) {
            //calculate how much of this product does customer needs
            int amountToBeBought =
                    (currentShoppingListItems + i)->amount - (currentShoppingListItems + i)->currentlyBoughtAmount;
            int totalBought = 0;
            //if no need to buy that category's items continue
            if (amountToBeBought == 0) {
                continue;
            } else {
                //if anyone else is buying that item at that time just continue to next category, you can buy it later.
                //else lock the item to don't let anyone buy it at the same time.
                if (pthread_mutex_trylock(&items_mutex[storeIndex][productIndex]) != 0) {
                    continue;
                } else {
                    sleep(1);
                    //this if and else are to calculate how much of that item is needed,
                    //in other words if stock is not enough make stock's current value 0
                    //and buy all items that are available and needed
                    if ((products + productIndex)->currentStock >= amountToBeBought) {

                        (currentShoppingListItems + i)->currentlyBoughtAmount += amountToBeBought;
                        (products + productIndex)->currentStock -= amountToBeBought;
                        totalBought = amountToBeBought;
                    } else {
                        (currentShoppingListItems + i)->currentlyBoughtAmount += (products +
                                                                                  productIndex)->currentStock;
                        totalBought = (products + productIndex)->currentStock;
                        (products + productIndex)->currentStock = 0;
                    }
                    pthread_mutex_unlock(&items_mutex[storeIndex][productIndex]);
                    sleep(1);
                }
            }

        }
    }

    printAfterBuyingOrFilling(person, storeStructs[storeIndex], previousShoppingListItems, currentShoppingListItems);
}

/* this function prints items that resident has in their shopping list while waiting in store to buy items. */
void printWhileWaiting(struct person *person, struct store *store) {
    char *output = malloc(1000);
    char *temp = malloc(100);
    snprintf(temp, 15 + (sizeof(t) / sizeof(int)), "At time t=%d, ", t);
    strcat(output, temp);
    if (person->role == RESIDENT) {
        snprintf(temp, 17 + sizeof(person->name), "%s wants to buy, ", person->name);
        strcat(output, temp);
        struct shoppingList *shoppingList = person->shoppingList;
        struct shoppingListItem *itemWishList = shoppingList->items;
        int countPrinted = 0;
        for (int i = 0; i < shoppingList->totalCategories; i++) {
            int amount = (itemWishList + i)->amount - (itemWishList + i)->currentlyBoughtAmount;
            if (amount > 0) {
                if (countPrinted > 0) {
                    strcat(output, ",");
                }
                countPrinted++;
                snprintf(temp, 5 + (sizeof(t) / sizeof(int)), " %d ", amount);
                strcat(output, temp);
                strcat(output, (itemWishList + i)->name);
            }
        }

        snprintf(temp, 11 + sizeof(store->name), " from %s.\n", store->name);
        strcat(output, temp);
    } else {
        snprintf(temp, 12 + sizeof(person->name), "%s visits ", person->name);
        strcat(output, temp);
        snprintf(temp, 10 + sizeof(store->name), "%s.\n", store->name);
        strcat(output, temp);
    }
    printf("%s", output);
    free(output);
    free(temp);
}


/* this function refills the items on each store and lock
each item in the store while this function is executing */
void *restockItems(struct person *person, int storeIndex) {
    struct store *store = storeStructs[storeIndex];
    int categoryCount = store->totalCategories;
    struct productCategory *productsOfStore = store->products;
    pthread_mutex_trylock(&items_mutex[storeIndex][0]);
    pthread_mutex_trylock(&items_mutex[storeIndex][1]);
    pthread_mutex_trylock(&items_mutex[storeIndex][2]);
    for (int i = 0; i < categoryCount; i++) {
        (productsOfStore + i)->currentStock = (productsOfStore + i)->maxStock;
    }

    printAfterBuyingOrFilling(person, storeStructs[storeIndex], NULL, NULL);
    pthread_mutex_unlock(&items_mutex[storeIndex][0]);
    pthread_mutex_unlock(&items_mutex[storeIndex][1]);
    pthread_mutex_unlock(&items_mutex[storeIndex][2]);
    sleep(1);
}

/* this function uses semaphores to block access of
people to store 1 if they are more than 3 */
void *visitStore1(struct person *ptr, int store) {
    storeStructs[store]->currentVisitors++;
    sem_wait(store1_semaphore);
    printWhileWaiting(ptr, storeStructs[store]);
    t++;
    ptr->visitedStores[store] = 1;
    if (ptr->role == RESIDENT) {
        buyItems(ptr, store);
    } else {
        restockItems(ptr, store);
    }
    sleep(1 + (rand() % 4));
    sem_post(store1_semaphore);
    storeStructs[store]->currentVisitors--;
    return NULL;
}

/* this function uses semaphores to block access of
people to store 2 if they are more than 3 */
void *visitStore2(struct person *ptr, int store) {
    printWhileWaiting(ptr, storeStructs[store]);
    sem_wait(store2_semaphore);
    t++;
    ptr->visitedStores[store] = 1;
    if (ptr->role == RESIDENT) {
        buyItems(ptr, store);
    } else {
        restockItems(ptr, store);
    }
    sleep(1 + (rand() % 4));
    sem_post(store2_semaphore);
    storeStructs[store]->currentVisitors--;
    return NULL;
}

/* this function uses semaphores to block access of
people to store 3 if they are more than 3 */
void *visitStore3(struct person *ptr, int store) {
    storeStructs[store]->currentVisitors++;
    sem_wait(store3_semaphore);
    printWhileWaiting(ptr, storeStructs[store]);
    t++;
    ptr->visitedStores[store] = 1;
    if (ptr->role == RESIDENT) {
        buyItems(ptr, store);
    } else {
        restockItems(ptr, store);
    }
    sleep(1 + (rand() % 4));
    sem_post(store3_semaphore);
    storeStructs[store]->currentVisitors--;
    return NULL;
}

/*
    if all stores are visited, returns -1
    else, returns index of the first not visited store
*/
int *areAllStoresVisited(struct person *person) {
    int *visitedStores = person->visitedStores;
    int *output = malloc(sizeof(int) * (STORE_COUNT + 1));

    for (int i = 0; i < STORE_COUNT + 1; i++) *(output + i) = -1;

    int store_counter = 0;
    for (int i = 0; i < STORE_COUNT; i++) {
        if (*(visitedStores + i) == 0) {
            output[store_counter] = i;
        }
    }

    return output;
}


/* clear the stores visited by that person after completing the list */
void clearVisitedStores(struct person *person) {
    for (int i = 0; i < STORE_COUNT; i++) {
        person->visitedStores[i] = 0;
    }
}

/* checks whether list is completed or not. if it is completed return 1, else, return 0. */
int isListCompleted(struct person *person) {
    struct shoppingList *shoppingList = person->shoppingList;
    struct shoppingListItem *itemWishList = shoppingList->items;
    for (int i = 0; i < shoppingList->totalCategories; i++) {
        if ((itemWishList + i)->amount - (itemWishList + i)->currentlyBoughtAmount != 0) {
            return 0;
        }
    }
    return 1;
}

/* this function decides which store should employee or resident go */
void *visitStore(struct person *ptr, int store) {
    if (store == 0) {
        visitStore1(ptr, store);
    } else if (store == 1) {
        visitStore2(ptr, store);
    } else {
        visitStore3(ptr, store);
    }
}


/* thread running function */
void *runner(void *ptr) {
    struct person *person = ptr;
    while (1) {
        int *visitArray;
        if (person->role == RESIDENT) {
            while (1) {
                if (isListCompleted(person)) break;
                int randomStore = rand() % 3;
                if (randomStore == 0) {
                    if (person->visitedStores[0] == 0) {
                        if (storeStructs[0]->currentVisitors < 3) {
                            if (isListCompleted(person)) break;
                            visitStore(person, 0);
                            continue;
                        }
                    }
                }
                if (randomStore == 1) {
                    if (person->visitedStores[1] == 0) {
                        if (storeStructs[1]->currentVisitors < 3) {
                            if (isListCompleted(person)) break;

                            visitStore(person, 1);
                            continue;
                        }
                    }

                }
                if (randomStore == 2) {
                    if (person->visitedStores[2] == 0) {
                        if (storeStructs[2]->currentVisitors < 3) {
                            if (isListCompleted(person)) break;
                            visitStore(person, 2);
                            continue;
                        }
                    }
                }

                if (person->visitedStores[0] == 1 && person->visitedStores[1] == 1 && person->visitedStores[2] == 1) {
                    break;
                }
            }
            if (isListCompleted(person)) {
                clearVisitedStores(person);
                person->completedLists++;
                if (person->completedLists == NUMBER_OF_LISTS) {
                    printf("%s has completed the lists. \nExiting... \n", person->name);
                    currentResidentCount--;
                    pthread_exit(0);
                } else {
                    sleep(3 + (rand() % RESIDENT_SLEEP_1));
                    person->shoppingList = createShoppingList();
                }

            } else {
                clearVisitedStores(person);
                sleep(3 + (rand() % RESIDENT_SLEEP_2));
            }
        }
            /* IF PERSON IS AN EMPLOYEE */
        else {
            int counter1 = 0;
            while (1) {
                visitArray = areAllStoresVisited(person);
                if (visitArray[0] == -1) {
                    break;
                } else if (visitArray[counter1] == -1) {

                    counter1 = 0;
                    continue;
                }

                if (storeStructs[counter1]->currentVisitors >= 3) {
                    counter1++;
                    continue;
                }
                if (currentResidentCount == 0) {
                    sem_close(store1_semaphore); /* close semaphore */
                    sem_close(store2_semaphore); /* close semaphore */
                    sem_close(store3_semaphore); /* close semaphore */
                    for (int i = 0; i < STORE_COUNT; i++) {
                        for (int j = 0; j < 3; j++) {
                            pthread_mutex_destroy(&items_mutex[i][j]);
                        }
                    }
                    exit(0);
                }
                visitStore(person, visitArray[counter1]);
                free(visitArray);
            }
            sleep(rand() % EMPLOYEE_SLEEP);
            clearVisitedStores(person);
        }
    }
}
