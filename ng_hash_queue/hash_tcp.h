#ifndef _HASH_TCP_H_
#define _HASH_TCP_H_

#ifdef _KERNEL
    #include "hash.h"
#endif

#define HASH_TCP_SIZE_INCREASE 1
#define HASH_TCP_SIZE_DECREASE 0

#define MIN_TCP_SIZE (1 << 8) //256. Размер должен быть степенью двойки!
#define MAX_TCP_SIZE (1 << 14) //16384. Размер должен быть степенью двойки!

/*
 * Вычисление хеша. 
 */
#define TCP_HASH(saddr,sport,daddr,dport,mask)		( (((const u_int32_t )(saddr)) \
							^((const u_int32_t )(sport)) \
							^((const u_int32_t )(daddr)) \
							^((const u_int32_t )(dport)))&(mask) )

// определения структур данных

/*
*структура элемента таблицы, в которой 
*хранятся данные о паре ключ-значение,
*указатель на следующий элемент очереди
*/
struct tcp_hash_el {
    u_int32_t hash; //хеш значение
    int value; //номер очереди, в которую необходимо отправить пакет
};

/*
*Cтруктура, описывающая хэш-таблицу.
*Инкапсулирует в себе массив указателей на очереди
*элементов, размер таблицы, количество элементов и фактор
*загрузки
*/
struct tcp_hash_table {
    struct tcp_hash_el **elements; //элементы таблицы
    size_t hash_size;	//текущий размер
    unsigned int hashMask; //маска для хеширования
    int num_elements; //счетчик количества элементов
    double loadfactor; //параметр загрузки таблицы
};

// прототипы функций

/*
 *Инициализация хэш-таблицы заданным значением
 */
int init_tcp_hash_table __P((struct tcp_hash_table*, size_t ));

/*
 * Инициализация по умолчанию
 */
int init_default_tcp_hash_table __P((struct tcp_hash_table*));

/*
 *Функция удаления памяти, используемой под таблицу
 */
int delete_tcp_hash_table __P((struct tcp_hash_table*));

/*
 *Добавление элемента в хэш таблицу. В случае удачного добавления возвращает ноль
 */
int add_tcp_hash_element __P((struct tcp_hash_table*, u_int32_t hash, int));

/*
 * Создает элемент в хеш таблице с заданным ключом и значением
 */
struct tcp_hash_el* create_tcp_hash_element __P((u_int32_t, int));

/**
 * Функция проверяет размер и если надо изменить размер хеш таблицы,
 * то вызывает функцию rehash();
 */   
void try_rehash_tcp(struct tcp_hash_table *t);

/*Функция удаления элемента с заданным хэшем из таблицы. 
 * В случае успеха возвращает ноль
 */
int remove_tcp_hash_element __P((struct tcp_hash_table*, u_int32_t));

/*
 * Удаляет все элементы
 */

void remove_all_tcp_hash_elements __P((struct tcp_hash_table *));

/*
 * Функция возвращает значение записи в таблице по ключу.
 * В контексте хеширования мас адресов это номер интерфейса, на
 * который необходимо отправить пакет.
 * Возвращает -1 в случае отсутствия
 */
int get_queue_num __P((struct tcp_hash_table*, u_int32_t));

/*
 *Функция перехэширования и изменения размера таблицы
 *Возвращает ноль в случае успешного выполнения
 */
int rehash_tcp_hash_table __P((struct tcp_hash_table *, int));

/*
 *Функция изменения параметра загрузки. В случае превышения этого параметра,
 *происходит изменение размера таблицы; Возвращает 0 в случае успеха;
 */
int setloadfactor_tcp_hash_table __P((struct tcp_hash_table *, double));

#endif /* _HASH_TCP_H_ */
