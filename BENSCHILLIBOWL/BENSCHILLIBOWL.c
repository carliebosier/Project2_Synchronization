#include "BENSCHILLIBOWL.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

MenuItem BENSCHILLIBOWLMenu[] = { 
    "BensChilli", 
    "BensHalfSmoke", 
    "BensHotDog", 
    "BensChilliCheeseFries", 
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
int BENSCHILLIBOWLMenuLength = 10;

/* Select a random item from the Menu and return it */
MenuItem PickRandomMenuItem() {
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }
    int index = rand() % BENSCHILLIBOWLMenuLength;
    return BENSCHILLIBOWLMenu[index];
}

/* Allocate memory for the Restaurant, then create the mutex and condition variables needed to instantiate the Restaurant */

BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    // Allocate memory for the restaurant
    BENSCHILLIBOWL* bcb = (BENSCHILLIBOWL*)malloc(sizeof(BENSCHILLIBOWL));
    if (bcb == NULL) {
        return NULL;
    }
    
    // Initialize restaurant fields
    bcb->orders = NULL;
    bcb->current_size = 0;
    bcb->max_size = max_size;
    bcb->next_order_number = 0;
    bcb->orders_handled = 0;
    bcb->expected_num_orders = expected_num_orders;
    
    // Initialize mutex
    if (pthread_mutex_init(&bcb->mutex, NULL) != 0) {
        free(bcb);
        return NULL;
    }
    
    // Initialize condition variables
    if (pthread_cond_init(&bcb->can_add_orders, NULL) != 0) {
        pthread_mutex_destroy(&bcb->mutex);
        free(bcb);
        return NULL;
    }
    
    if (pthread_cond_init(&bcb->can_get_orders, NULL) != 0) {
        pthread_cond_destroy(&bcb->can_add_orders);
        pthread_mutex_destroy(&bcb->mutex);
        free(bcb);
        return NULL;
    }
    
    printf("Restaurant is open!\n");
    return bcb;
}


/* check that the number of orders received is equal to the number handled (ie.fullfilled). Remember to deallocate your resources */

void CloseRestaurant(BENSCHILLIBOWL* bcb) {
    // At this point, all customer and cook threads have completed
    // Verify that orders_handled matches expected_num_orders
    pthread_mutex_lock(&bcb->mutex);
    assert(bcb->orders_handled == bcb->expected_num_orders);
    
    // Verify that the queue is empty
    assert(bcb->current_size == 0);
    pthread_mutex_unlock(&bcb->mutex);
    
    // Destroy condition variables
    pthread_cond_destroy(&bcb->can_add_orders);
    pthread_cond_destroy(&bcb->can_get_orders);
    
    // Destroy mutex
    pthread_mutex_destroy(&bcb->mutex);
    
    // Free the restaurant
    free(bcb);
    
    printf("Restaurant is closed!\n");
}

/* add an order to the back of queue */
int AddOrder(BENSCHILLIBOWL* bcb, Order* order) {
    // Lock the mutex
    pthread_mutex_lock(&bcb->mutex);
    
    // Wait until the restaurant is not full
    while (IsFull(bcb)) {
        pthread_cond_wait(&bcb->can_add_orders, &bcb->mutex);
    }
    
    // Assign order number
    order->order_number = bcb->next_order_number;
    bcb->next_order_number++;
    
    // Add order to the back of the queue
    AddOrderToBack(&bcb->orders, order);
    bcb->current_size++;
    
    // Signal that orders are available
    pthread_cond_signal(&bcb->can_get_orders);
    
    // Unlock the mutex
    pthread_mutex_unlock(&bcb->mutex);
    
    return order->order_number;
}

/* remove an order from the queue */
Order *GetOrder(BENSCHILLIBOWL* bcb) {
    // Lock the mutex
    pthread_mutex_lock(&bcb->mutex);
    
    // Wait until the restaurant is not empty
    // Also wait if all orders haven't been placed yet (next_order_number < expected_num_orders)
    // This ensures we don't exit early if customers are still placing orders
    while (IsEmpty(bcb) && bcb->next_order_number < bcb->expected_num_orders) {
        pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex);
    }
    
    // Check if there are no more orders and all orders have been placed
    if (IsEmpty(bcb)) {
        // All orders have been placed and queue is empty, so no more orders
        // Signal other cooks that there are no orders left
        pthread_cond_broadcast(&bcb->can_get_orders);
        pthread_mutex_unlock(&bcb->mutex);
        return NULL;
    }
    
    // Get order from the front of the queue
    Order* order = bcb->orders;
    bcb->orders = bcb->orders->next;
    bcb->current_size--;
    bcb->orders_handled++;
    
    // Signal that space is available for new orders
    pthread_cond_signal(&bcb->can_add_orders);
    
    // Unlock the mutex
    pthread_mutex_unlock(&bcb->mutex);
    
    return order;
}

// Optional helper functions (you can implement if you think they would be useful)
bool IsEmpty(BENSCHILLIBOWL* bcb) {
    return bcb->current_size == 0;
}

bool IsFull(BENSCHILLIBOWL* bcb) {
    return bcb->current_size >= bcb->max_size;
}

/* this methods adds order to rear of queue */
void AddOrderToBack(Order **orders, Order *order) {
    if (*orders == NULL) {
        // Queue is empty, make this the first order
        *orders = order;
        order->next = NULL;
    } else {
        // Find the last order in the queue
        Order *current = *orders;
        while (current->next != NULL) {
            current = current->next;
        }
        // Add the new order to the end
        current->next = order;
        order->next = NULL;
    }
}

