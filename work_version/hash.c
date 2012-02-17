/*
**********************************************************************
*      Функции для работы с хэш-таблицами
*  
*
* (c) Copyright
*---------------------------------------------------------------------
* Файл hash_funcs.c - модуль функций
* Автор: Лукашин Алексей
*
* $Id$
**********************************************************************

*/
#include "hash.h"


/**
 * функция добавления элемента.
 */
static int add_element_internal __P((struct hash_table *, const u_char* , int , int ));

/*
 *Функция инициализации хэш таблицы заданным размером
 *В случае удачного завершения возвращает ноль
 */
int init(struct hash_table *t, size_t size) {

    if (size < MIN_SIZE || size > MAX_SIZE) {
        size = MIN_SIZE;
    }

	if ((N_MALLOC(t->elements, struct hash_el** , size * sizeof(struct hash_el *)) ) == NULL) {
		return -1;
	}
	
	t->hash_size = size;
	t->num_elements = 0;
	t->loadfactor = 0.9;
	t->hashMask = size - 1;
	return 0;
}

/*
 * Инициализация по умолчанию
 */
int init_default(struct hash_table *t) {
	return init(t, MIN_SIZE);
}

/*
 *Функция изменения параметра загрузки. В случае превышения этого параметра,
 *происходит изменение размера таблицы;
 */
int setloadfactor(struct hash_table *t, double loadfactor) {
	if(loadfactor <= 0 || loadfactor > 1) {
		return -1;
	}
	t->loadfactor = loadfactor;
	return 0;
}

/*
 *Освобождение памяти, занятой под хэш таблицу
 */
int delete_table(struct hash_table *t) {
    int i = 0;
	struct hash_el* el;
	struct hash_el *head;
	for(i = 0; i < t->hash_size; i++) {
		head = t->elements[i];
		for(el = head; el != NULL; el=el->next) {
			N_FREE(el->key);
			N_FREE(el);
		}
	}

	N_FREE(t->elements);
	return 0;
}


/*
 * Функция добавления элемента в хэш таблицу.
 *  -1 В случае неудачи.
 *  -2 Eсли заменен элемент.
 *  TODO Вынести в константы
 */
int add_element(struct hash_table *t, const u_char* key, int value) {
	return add_element_internal(t, key, value, 1);
}

/*
 * Функция добавления элемента в хэш таблицу.
 *  -1 В случае неудачи.
 *  -2 Eсли заменен элемент.
 *  TODO Вынести в константы
 * isCheck - если флаг выставлен, то необходимо проверять перехеширование
 */
static int add_element_internal(struct hash_table *t, const u_char* key, int value, int isCheck) {
		if (t->num_elements >= MAX_SIZE) {
		return -1;
	}

    unsigned int hash = HASH(key,t->hashMask);

    //Теперь требуется взять элемент из таблицы, если его нет, то добавить, если есть, то добавить
    //в очередь
    struct hash_el *el = t->elements[hash];
	struct hash_el* new;

    if(el != NULL) {
		//проверить на наличие такого элемента и добавить в очередь
		struct hash_el *e;
		for(e = el; e->next != NULL; e = e->next) {
			if(ETHER_EQUAL(e->key, key)) {
				if (el->value == value) {
					return -1; // Мы добавляем уже существующий элемент
				} else {
					el->value = value; //заменить элемент в таблице
					return -2;
				}
			}
		}

		//итак, остался хвост очереди -  проверим его, не равен ли его
		//ключ нашему ключу и если нет, то добавим в хвост элемент

		if(ETHER_EQUAL(e->key, key)) {
			if (el->value == value) {
                   return -1; // Мы добавляем уже новый элемент
            } else {
            	el->value = value; //заменить элемент в таблице
            	return -2;
           }
		}

		//Создадим элемент для записи
		new = create_element(key, value, hash);
		if (new == NULL){
			return -1; //ошибка выделения памяти
		}
		e->next = new;

    } else {
		//Создадим элемент для записи
		new = create_element(key, value, hash);
        if (new == NULL){
            return -1; //ошибка выделения памяти
        }
    	t->elements[hash] = new;
	}

    t->num_elements++;

	if (isCheck) {
		try_rehash(t);
	}
    return 0;
}

/**
 * Функция проверяет размер и если надо изменить размер хеш таблицы,
 * то вызывает функцию rehash();
 */
void try_rehash(struct hash_table *t) {
	double factor = (double) t->num_elements / t->hash_size;
	if( (factor > t->loadfactor) && (t->hash_size < MAX_SIZE)) {
        rehash(t, HASH_SIZE_INCREASE);
    } 
/*
	else if (t->hash_size > MIN_SIZE && (factor < (1.0 - t->loadfactor)) ){
        rehash(t, HASH_SIZE_DECREASE);
    }
*/
}

/*
 * Создает элемент в хеш таблице с заданным ключом и значением
 */
struct hash_el* create_element(const u_char* key, int value, int hash) {
    struct hash_el *new_el;

    if(N_MALLOC(new_el, struct hash_el*, sizeof(struct hash_el)) == NULL ){
        return NULL;
    }

    if(N_MALLOC(new_el->key, u_char*, ETHER_ADDR_LEN * sizeof(u_char)) == NULL) {
        return NULL;
    }

    memcpy(new_el->key, key, ETHER_ADDR_LEN);

    //Записать номер интерфейса
    new_el->value = value;
    new_el->hash = hash; // запомнить хеш
    new_el->next = NULL; // хвостика нет

    return new_el;
}


/*
 *Функция удаления элемента из таблицы. В случае успеха возвращает 0
 */
int remove_element(struct hash_table *t, u_char* key) {

    unsigned int hash = HASH(key, t->hashMask);

    struct hash_el* head = *(t->elements + hash);
    struct hash_el* el = head;
    struct hash_el* prev = NULL;

    //Поиск элементов в очереди
    while(el != NULL) {
        if(ETHER_EQUAL(el->key, key)) {

            if(prev != NULL) {
                prev->next = el->next;
            } else {
                *(t->elements + hash) = el->next;
            }

			N_FREE(el->key);
			N_FREE(el);

            t->num_elements--;

			try_rehash(t);

			return 0;
        }
        prev = el;
        el = el->next;
    }
    return -1;
}

//Удаление всех элементов
void remove_all_elements (struct hash_table *t) {
	int i;
    struct hash_el *el;
	struct hash_el *queue;
	struct hash_el *old;

	for(i = 0; i < t->hash_size; i++) {
		queue = t->elements[i];
		if (queue != NULL) {
			for(el = queue; el != NULL;) {
				old = el;
				el = el->next;				
				N_FREE(old->key);
				N_FREE(old);
				t->num_elements--;
			}
			t->elements[i] = NULL;
		}
	}
}

/*
 *Функция возвращает очередь элементов которые имеют хэш, соответсвующий key,
 *NULL  в случае отсутствия
 *В случае коллизии, очередь из более чем одного элемента
*/
struct hash_el* get_elements(struct hash_table *t, u_char* key) {

    unsigned int hash = HASH(key, t->hashMask);

    struct hash_el* head = t->elements[hash];
    struct hash_el* el;

    struct hash_el* result = NULL;

    for(el = head; el != NULL; el = el->next) {
        if(el->hash == hash) {
            if(result == NULL) {
                result = el;
            } else {
                result->next = el;
            }
        }
    }

    return result;
}

/*
 * Функция возвращает значение записи в таблице по ключу.
 * В контексте хеширования мас адресов это номер интерфейса, на 
 * который необходимо отправить пакет.
 * Возвращает -1 в случае отсутствия
 */
int get_element(struct hash_table *t, u_char* key) {

    unsigned int hash = HASH(key, t->hashMask);

    struct hash_el* head = t->elements[hash];
    struct hash_el* el;

    for(el = head; el != NULL; el = el->next) {
        if(ETHER_EQUAL(el->key, key)) {
			return el->value;
        }
    }

    return -1;
}


/*
 *Функция возвращает очередь с ключом key,
 *NULL  в случае отсутствия
*/
struct hash_el* get_queue_by_key(struct hash_table *t, u_char* key) {

    unsigned int hash = HASH(key, t->hashMask);
    struct hash_el* head = t->elements[hash];
    struct hash_el* el;

    struct hash_el* result = NULL;
	struct hash_el* marker = NULL;

    for(el = head; el != NULL; el = el->next) {
        if(ETHER_EQUAL(el->key, key)) {
    		if(result == NULL) {
				result = el;
				marker = result;
			} else {
				marker->next = el;
				marker = el;
			}
		}
    }

	if(marker != NULL)
		marker->next = NULL;
    return result;
}


/*
 *Функция перехэширования и изменения размера таблицы
 *Возвращает ноль в случае успешного выполнения
 */
int rehash (struct hash_table *t, int direction) {

	if(t->hash_size >= MAX_SIZE) {
		return -1;
    }

	int oldsize = t->hash_size;
	int newsize;

	if(direction == HASH_SIZE_DECREASE) {
		newsize = t->hash_size >> 1;
		newsize = (newsize < MIN_SIZE) ? MIN_SIZE: newsize;
	} else if(direction == HASH_SIZE_INCREASE) {
		newsize = t->hash_size << 1;
		newsize = (newsize > MAX_SIZE) ? MAX_SIZE: newsize;
	} else {
		return -1;
	}

	struct hash_el **oldelements = t->elements;
	struct hash_el **newelements;
	
//Выделим память для новой таблицы
	if(N_MALLOC(newelements, struct hash_el**, newsize*sizeof(struct hash_el*)) == NULL) {
		return -1;
	}	
	printf("rehashing to %d", newsize);

	t->elements = newelements;
	t->hash_size = newsize;
	t->num_elements = 0;
	t->hashMask = newsize - 1;
	
	int i;
	struct hash_el* el;

//перенесем элементы из старой в новую
	struct hash_el* queue;
	for(i = 0; i < oldsize; i++) {
        queue = oldelements[i];
		if(queue != NULL) {
			for(el = queue; el != NULL; el = el->next) {
   				add_element_internal(t, el->key, el->value, 0);
			}

		}
	}
//Освободим старyю таблицу
	N_FREE(oldelements);

	return 0;
}

