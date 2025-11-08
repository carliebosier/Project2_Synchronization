#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "BENSCHILLIBOWL.h"

// Feel free to play with these numbers! This is a great way to
// test your implementation.
#define BENSCHILLIBOWL_SIZE 100
#define NUM_CUSTOMERS 90
#define NUM_COOKS 10
#define ORDERS_PER_CUSTOMER 3
#define EXPECTED_NUM_ORDERS NUM_CUSTOMERS * ORDERS_PER_CUSTOMER

// Global variable for the restaurant.
BENSCHILLIBOWL *bcb;

/**
 * Thread funtion that represents a customer. A customer should:
 *  - allocate space (memory) for an order.
 *  - select a menu item.
 *  - populate the order with their menu item and their customer ID.
 *  - add their order to the restaurant.
 */
void* BENSCHILLIBOWLCustomer(void* tid) {
    int customer_id = (int)(long) tid;
    
    // Each customer places ORDERS_PER_CUSTOMER orders
    for (int i = 0; i < ORDERS_PER_CUSTOMER; i++) {
        // Allocate space for an order
        Order* order = (Order*)malloc(sizeof(Order));
        if (order == NULL) {
            return NULL;
        }
        
        // Select a menu item and save it locally (menu items are static strings, so this is safe)
        MenuItem menu_item = PickRandomMenuItem();
        order->menu_item = menu_item;
        
        // Populate the order with customer ID
        order->customer_id = customer_id;
        order->next = NULL;
        
        // Add order to the restaurant (order might be freed immediately by a cook)
        int order_number = AddOrder(bcb, order);
        
        // Print using the local variable, not the order pointer (which might be freed)
        printf("Customer #%d ordered %s (Order #%d)\n", customer_id, menu_item, order_number);
    }
    
	return NULL;
}

/**
 * Thread function that represents a cook in the restaurant. A cook should:
 *  - get an order from the restaurant.
 *  - if the order is valid, it should fulfill the order, and then
 *    free the space taken by the order.
 * The cook should take orders from the restaurants until it does not
 * receive an order.
 */
void* BENSCHILLIBOWLCook(void* tid) {
    int cook_id = (int)(long) tid;
	int orders_fulfilled = 0;
	
	// Cook continues to get orders until no order is returned
	while (1) {
	    Order* order = GetOrder(bcb);
	    
	    // If order is NULL, there are no more orders
	    if (order == NULL) {
	        break;
	    }
	    
	    // Fulfill the order
	    printf("Cook #%d fulfilled Order #%d: %s for Customer #%d\n", 
	           cook_id, order->order_number, order->menu_item, order->customer_id);
	    
	    // Free the space taken by the order
	    free(order);
	    orders_fulfilled++;
	}
	
	printf("Cook #%d fulfilled %d orders\n", cook_id, orders_fulfilled);
	return NULL;
}

/**
 * Runs when the program begins executing. This program should:
 *  - open the restaurant
 *  - create customers and cooks
 *  - wait for all customers and cooks to be done
 *  - close the restaurant.
 */
int main() {
    // Open the restaurant
    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE, EXPECTED_NUM_ORDERS);
    if (bcb == NULL) {
        fprintf(stderr, "Failed to open restaurant\n");
        return 1;
    }
    
    // Create arrays to store thread IDs
    pthread_t customer_threads[NUM_CUSTOMERS];
    pthread_t cook_threads[NUM_COOKS];
    
    // Create customer threads
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        if (pthread_create(&customer_threads[i], NULL, BENSCHILLIBOWLCustomer, (void*)(long)i) != 0) {
            fprintf(stderr, "Failed to create customer thread %d\n", i);
            return 1;
        }
    }
    
    // Create cook threads
    for (int i = 0; i < NUM_COOKS; i++) {
        if (pthread_create(&cook_threads[i], NULL, BENSCHILLIBOWLCook, (void*)(long)i) != 0) {
            fprintf(stderr, "Failed to create cook thread %d\n", i);
            return 1;
        }
    }
    
    // Wait for all customer threads to complete
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        if (pthread_join(customer_threads[i], NULL) != 0) {
            fprintf(stderr, "Failed to join customer thread %d\n", i);
            return 1;
        }
    }
    
    // Signal all cooks that no more orders will be added
    // This is done by broadcasting the condition variable after all customers are done
    pthread_mutex_lock(&bcb->mutex);
    pthread_cond_broadcast(&bcb->can_get_orders);
    pthread_mutex_unlock(&bcb->mutex);
    
    // Wait for all cook threads to complete
    for (int i = 0; i < NUM_COOKS; i++) {
        if (pthread_join(cook_threads[i], NULL) != 0) {
            fprintf(stderr, "Failed to join cook thread %d\n", i);
            return 1;
        }
    }
    
    // Close the restaurant
    CloseRestaurant(bcb);
    
    return 0;
}
