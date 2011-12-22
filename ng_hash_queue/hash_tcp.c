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
#include "hash_tcp.h"


/**
 * функция добавления элемента.
 */
static int add_tcp_hash_element_internal __P((struct tcp_hash_table *, u_int32_t hash , int , int ));

/*
 *Функция инициализации хэш таблицы заданным размером
 *В случае удачного завершения возвращает ноль
 */
int init_tcp_hash_table(struct tcp_hash_table *t, size_t size) {

    if (size < MIN_TCP_SIZE || size > MAX_TCP_SIZE) {
        size = MIN_TCP_SIZE;
    }

	if ((N_MALLOC(t->elements, struct tcp_hash_el** , size * sizeof(struct tcp_hash_el*)) ) == NULL) {
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
int init_default_tcp_hash_table(struct tcp_hash_table *t) {
	return init_tcp_hash_table(t, MIN_TCP_SIZE);
}

/*
 *Функция изменения параметра загрузки. В случае превышения этого параметра,
 *происходит изменение размера таблицы;
 */
int setloadfactor_tcp_hash_table(struct tcp_hash_table *t, double loadfactor) {
	if(loadfactor <= 0 || loadfactor > 1) {
		return -1;
	}
	t->loadfactor = loadfactor;
	return 0;
}

/*
 *Освобождение памяти, занятой под хэш таблицу
 */
int delete_tcp_hash_table(struct tcp_hash_table *t) {
    int i = 0;
	struct tcp_hash_el* el;
	for(i = 0; i < t->hash_size; i++) {
		el = t->elements[i];
		N_FREE(el);
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
int add_tcp_hash_element(struct tcp_hash_table *t, u_int32_t hash, int value) {
	return add_tcp_hash_element_internal(t, hash, value, 1);
}

/*
 * Функция добавления элемента в хэш таблицу.
 *  -1 Мы добавляем уже существующий элемент.
 *  -2 Eсли заменен элемент.
 *  -3 Ошибка выделения памяти.
 *  TODO Вынести в константы
 * isCheck - если флаг выставлен, то необходимо проверять перехеширование
 */
static int add_tcp_hash_element_internal(struct tcp_hash_table *t, u_int32_t hash, int value, int isCheck) {
		if (t->num_elements >= MAX_TCP_SIZE) {
		return -1;
	}

    //Теперь требуется взять элемент из таблицы, если его нет, то добавить, если есть, то добавить
    //в очередь
    struct tcp_hash_el *el = t->elements[hash];
	struct tcp_hash_el* new;

    if(el != NULL) {
	if (el->value == value) {
		return -1; // Мы добавляем уже существующий элемент
	} else {
		el->value = value; //заменить элемент в таблице
		return -2;
	}
    
    } else {
	//Создадим элемент для записи
	new = create_tcp_hash_element(hash, value);
        if (new == NULL){
            return -3; //ошибка выделения памяти
        }
    	t->elements[hash] = new;
    }

    t->num_elements++;

	if (isCheck) {
		try_rehash_tcp(t);
	}
    return 0;
}

/**
 * Функция проверяет размер и если надо изменить размер хеш таблицы,
 * то вызывает функцию rehash();
 */
void try_rehash_tcp(struct tcp_hash_table *t) {
	double factor = (double) t->num_elements / t->hash_size;
	if( (factor > t->loadfactor) && (t->hash_size < MAX_TCP_SIZE)) {
        rehash_tcp_hash_table(t, HASH_TCP_SIZE_INCREASE);
    } 
/*
	else if (t->hash_size > MIN_TCP_SIZE && (factor < (1.0 - t->loadfactor)) ){
        rehash(t, HASH_TCP_SIZE_DECREASE);
    }
*/
}

/*
 * Создает элемент в хеш таблице с заданным ключом и значением
 */
struct tcp_hash_el* create_tcp_hash_element(u_int32_t hash, int value) {
    struct tcp_hash_el *new_el;

    if(N_MALLOC(new_el, struct tcp_hash_el*, sizeof(struct tcp_hash_el)) == NULL ){
      return NULL;
    }
    //Записать номер очереди
    new_el->value = value;
    new_el->hash = hash; // запомнить хеш
    return new_el;
}


/*
 *Функция удаления элемента из таблицы. В случае успеха возвращает 0
 */
int remove_tcp_hash_element(struct tcp_hash_table *t, u_int32_t hash) {


    struct tcp_hash_el* el = *(t->elements + hash);
    N_FREE(el);
	t->elements[hash] = NULL;
	
	t->num_elements--;

	try_rehash_tcp(t);

	return 0;
}

//Удаление всех элементов
void remove_all_tcp_hash_elements (struct tcp_hash_table *t) {
	int i;
   
	for(i = 0; i < t->hash_size; i++) {
		N_FREE(t->elements[i]);
		t->elements[i] = NULL;
	}
}


/*
 * Функция возвращает значение записи в таблице по ключу.
 * В контексте хеширования мас адресов это номер интерфейса, на 
 * который необходимо отправить пакет.
 * Возвращает -1 в случае отсутствия
 */
int get_queue_num(struct tcp_hash_table *t, u_int32_t hash) {
    struct tcp_hash_el* el = t->elements[hash];
	if (el != NULL)
		return el->value;
	else
		return -1;
}

/*
 *Функция перехэширования и изменения размера таблицы
 *Возвращает ноль в случае успешного выполнения
 */
int rehash_tcp_hash_table (struct tcp_hash_table *t, int direction) {

	if(t->hash_size >= MAX_TCP_SIZE) {
		return -1;
    }

	int oldsize = t->hash_size;
	int newsize;

	if(direction == HASH_TCP_SIZE_DECREASE) {
		newsize = t->hash_size >> 1;
		newsize = (newsize < MIN_TCP_SIZE) ? MIN_TCP_SIZE: newsize;
	} else if(direction == HASH_TCP_SIZE_INCREASE) {
		newsize = t->hash_size << 1;
		newsize = (newsize > MAX_TCP_SIZE) ? MAX_TCP_SIZE: newsize;
	} else {
		return -1;
	}

	struct tcp_hash_el **oldelements = t->elements;
	struct tcp_hash_el **newelements;
	
//Выделим память для новой таблицы
	if(N_MALLOC(newelements, struct tcp_hash_el**, newsize*sizeof(struct tcp_hash_el*)) == NULL) {
		return -1;
	}	
	printf("rehashing to %d", newsize);

	t->elements = newelements;
	t->hash_size = newsize;
	t->num_elements = 0;
	t->hashMask = newsize - 1;
	
	int i;
	struct tcp_hash_el* el;

//перенесем элементы из старой в новую
	for(i = 0; i < oldsize; i++) {
        el = oldelements[i];
		if(el != NULL) {
			add_tcp_hash_element_internal(t, el->hash, el->value, 0);
		}
	}
//Освободим старyю таблицу
	N_FREE(oldelements);

	return 0;
}
