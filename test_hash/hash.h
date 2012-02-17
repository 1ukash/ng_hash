/*
**********************************************************************
*     Заголовочный файл, описывающий функции и структуры для работы с 
*                        хэш-таблицами
*
* (c) Copyright
*---------------------------------------------------------------------
* Файл hash.h - модуль определений
* Автор: Лукашин Алексей
*
**********************************************************************
*/

#ifndef _HASH_H_
#define _HASH_H_

//Необходимо определять для получения временных характеристик
#define COUNT_TIMINGS

//На это количество высчитываются средние характеристики
//для временных характеристик пакетов
#define NPKTS 1000

#include <sys/types.h>
#include <net/ethernet.h>

/*
 * Ниже определены макросы для однотипного 
 * выделения и освобождения памяти в ядре и контексте пользователя
 * Пользоваться так:
 *
 * N_MALLOC( переменная , тип , размер_в_байтах )
 * N_FREE( переменная )
 *
 */
#ifdef _KERNEL
/*
 * РЕЖИМ ЯДРА
 */
	#include "ng_hash.h"

	#define	N_MALLOC(space, cast, size) \
		((space) = (cast)malloc((u_long)(size), M_NETGRAPH, M_NOWAIT|M_ZERO))
	#define	N_FREE(addr) free((addr), M_NETGRAPH)

#else /* _KERNEL */
/*
 *  РЕЖИМ USERSPACE
 */
	#include <stdlib.h>

    #define N_MALLOC(space, cast, size) \
        ((space) = (cast)malloc((u_long)(size)))
    #define N_FREE(addr) free((addr))
#endif /* !_KERNEL */

#define HASH_SIZE_INCREASE 1
#define HASH_SIZE_DECREASE 0

#define MIN_SIZE (1 << 8) //256. Размер должен быть степенью двойки!
#define MAX_SIZE (1 << 14) //16384. Размер должен быть степенью двойки!

/* Сравнение адресов не по байтам, а при помощи 32 битного и 16 битного слов (6 байт итого) */
#define ETHER_EQUAL(a,b)	(((const u_int32_t *)(a))[0] \
					== ((const u_int32_t *)(b))[0] \
				    && ((const u_int16_t *)(a))[2] \
					== ((const u_int16_t *)(b))[2])

/*
 * Вычисление хеша. Хешируются только три байта адреса - номер сетевой карты. 
 * Другие три байта - номер производителя и 2 бита отведенные под специальные значения
 */
#define HASH(addr,mask)		( (((const u_int16_t *)(addr))[0] 	\
				 ^ ((const u_int16_t *)(addr))[1] 	\
				 ^ ((const u_int16_t *)(addr))[2]) & (mask) )

#define VC_HASH(saddr,sport,daddr,dport,mask)		( (((const u_int32_t)(saddr)) 	\
				 ^ ((const u_int32_t)(sport)) 	\
				 ^ ((const u_int32_t)(daddr))		\
				 ^ ((const u_int32_t)(dport))) & (mask) )
// определения структур данных

/*
*структура элемента таблицы, в которой 
*хранятся данные о паре ключ-значение,
*указатель на следующий элемент очереди
*/
struct hash_el {
    struct hash_el *next; //ссылка на следующий элемент
    unsigned int hash; //хеш значение
    int value; //номер линка на который необходимо отправить пакет
    u_char* key;
};

/*
*Cтруктура, описывающая хэш-таблицу.
*Инкапсулирует в себе массив указателей на очереди
*элементов, размер таблицы, количество элементов и фактор
*загрузки
*/
struct hash_table {
    struct hash_el **elements; //элементы таблицы
    size_t hash_size;	//текущий размер
    unsigned int hashMask; //маска для хеширования
    int num_elements; //счетчик количества элементов
    double loadfactor; //параметр загрузки таблицы
};

// прототипы функций

/*
 *Инициализация хэш-таблицы заданным значением
 */
int init __P((struct hash_table*, size_t ));

/*
 * Инициализация по умолчанию
 */
int init_default __P((struct hash_table*));

/*
 *Функция удаления памяти, используемой под таблицу
 */
int delete_table __P((struct hash_table*));

/*
 *Добавление элемента в хэш таблицу. В случае удачного добавления возвращает ноль
 */
int add_element __P((struct hash_table*, const u_char*, int));

/*
 * Создает элемент в хеш таблице с заданным ключом и значением
 */
struct hash_el* create_element __P((const u_char*, int, int));

/**
 * Функция проверяет размер и если надо изменить размер хеш таблицы,
 * то вызывает функцию rehash();
 */   
void try_rehash(struct hash_table *t);

/*Функция удаления элемента с заданным ключом из таблицы. 
 * В случае успеха возвращает ноль
 */
int remove_element __P((struct hash_table*, u_char*));

/*
 * Удаляет все элементы
 */

void remove_all_elements __P((struct hash_table *));

/*
 *функция для вывода очереди элементов таблицы. Происходит сравнение значений хэша
 *-> можно таким образом увидеть возникшую коллизию
 */
struct hash_el* get_elements __P((struct hash_table*, u_char*));

/*
 *Функция возвращает очередь с ключом key,
 *NULL  в случае отсутствия
*/
struct hash_el* get_queue_by_key __P((struct hash_table *, u_char* ));

/*
 * Функция возвращает значение записи в таблице по ключу.
 * В контексте хеширования мас адресов это номер интерфейса, на
 * который необходимо отправить пакет.
 * Возвращает -1 в случае отсутствия
 */
int get_element __P((struct hash_table*, u_char*));

/*
 *Функция перехэширования и изменения размера таблицы
 *Возвращает ноль в случае успешного выполнения
 */
int rehash __P((struct hash_table *, int));

/*
 *Функция изменения параметра загрузки. В случае превышения этого параметра,
 *происходит изменение размера таблицы; Возвращает 0 в случае успеха;
 */
int setloadfactor __P((struct hash_table *, double));

#endif /* _HASH_H_ */

