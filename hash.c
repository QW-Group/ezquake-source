
#include <stdio.h> // <-- only needed for Hash_BucketStats
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "quakedef.h"
#include "q_shared.h"
#include "hash.h"

#ifdef WITH_FTE_VFS

void Hash_InitTable(hashtable_t *table, int numbucks, void *mem)
{
	table->numbuckets = numbucks;
	table->bucket = (bucket_t **)mem;
}

/* http://www.cse.yorku.ca/~oz/hash.html
 * djb2
 * This algorithm (k=33) was first reported by dan bernstein many years ago
 * in comp.lang.c. another version of this algorithm (now favored by bernstein)
 * uses xor: hash(i) = hash(i - 1) * 33 ^ str[i]; the magic of number 33 (why
 * it works better than many other constants, prime or not) has never been 
 * adequately explained.
 */
int Hash_Key(char *name, int modulus) {
	unsigned int key = 5381;

	for (key = 5381; *name; name++)
		key = ((key << 5) + key) + *name; /* key * 33 + c */

	return (int) (key % modulus);
}
int Hash_KeyInsensative(char *name, int modulus) {
	unsigned int key = 5381;

	for (key = 5381; *name; name++)
		key = ((key << 5) + key) + tolower(*name); /* key * 33 + c */

	return (int) (key % modulus);
}

#if 0
int Hash_Key(char *name, int modulus)
{	//fixme: optimize.
	unsigned int key;
	for (key=0;*name; name++)
		key += ((key<<3) + (key>>28) + *name);
		
	return (int)(key%modulus);
}
int Hash_KeyInsensative(char *name, int modulus)
{	//fixme: optimize.
	unsigned int key;
	for (key=0;*name; name++)
	{
		if (*name >= 'A' && *name <= 'Z')
			key += ((key<<3) + (key>>28) + (*name-'A'+'a'));
		else
			key += ((key<<3) + (key>>28) + *name);
	}
		
	return (int)(key%modulus);
}
#endif

void *Hash_Get(hashtable_t *table, char *name)
{
	int bucknum = Hash_Key(name, table->numbuckets);
	bucket_t *buck;

	buck = table->bucket[bucknum];

	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}
void *Hash_GetInsensative(hashtable_t *table, char *name)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);
	bucket_t *buck;

	buck = table->bucket[bucknum];

	while(buck)
	{
		if (!strcasecmp(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}
void *Hash_GetKey(hashtable_t *table, char *key)
{
	int bucknum = ((int64_t) key) % table->numbuckets;
	bucket_t *buck;

	buck = table->bucket[bucknum];

	while(buck)
	{
		if (buck->keystring == key)
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}
void *Hash_GetNext(hashtable_t *table, char *name, void *old)
{
	int bucknum = Hash_Key(name, table->numbuckets);
	bucket_t *buck;

	buck = table->bucket[bucknum];

	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
		{
			if (buck->data == old)	//found the old one
				break;
		}

		buck = buck->next;
	}
	if (!buck)
		return NULL;

	buck = buck->next;//don't return old
	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}
void *Hash_GetNextInsensative(hashtable_t *table, char *name, void *old)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);
	bucket_t *buck;

	buck = table->bucket[bucknum];

	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
		{
			if (buck->data == old)	//found the old one
				break;
		}

		buck = buck->next;
	}
	if (!buck)
		return NULL;

	buck = buck->next;//don't return old
	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}


void *Hash_Add(hashtable_t *table, char *name, void *data, bucket_t *buck)
{
	int bucknum = Hash_Key(name, table->numbuckets);

	buck->data = data;
	buck->keystring = name;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}
void *Hash_AddInsensative(hashtable_t *table, char *name, void *data, bucket_t *buck)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);

	buck->data = data;
	buck->keystring = name;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}
void *Hash_AddKey(hashtable_t *table, char *key, void *data, bucket_t *buck)
{
	int bucknum = ((int64_t) key) % table->numbuckets;

	buck->data = data;
	buck->keystring = key;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void Hash_Remove(hashtable_t *table, char *name)
{
	int bucknum = Hash_Key(name, table->numbuckets);
	bucket_t *buck;	

	buck = table->bucket[bucknum];

	if (!STRCMP(name, buck->keystring))
	{
		table->bucket[bucknum] = buck->next;
		return;
	}


	while(buck->next)
	{
		if (!STRCMP(name, buck->next->keystring))
		{
			buck->next = buck->next->next;
			return;
		}

		buck = buck->next;
	}
	return;
}

void Hash_RemoveData(hashtable_t *table, char *name, void *data)
{
	int bucknum = Hash_Key(name, table->numbuckets);
	bucket_t *buck;	

	buck = table->bucket[bucknum];

	if (buck->data == data)
		if (!STRCMP(name, buck->keystring))
		{
			table->bucket[bucknum] = buck->next;
			return;
		}


	while(buck->next)
	{
		if (buck->next->data == data)
			if (!STRCMP(name, buck->next->keystring))
			{
				buck->next = buck->next->next;
				return;
			}

		buck = buck->next;
	}
	return;
}


void Hash_RemoveKey(hashtable_t *table, char *key)
{
	int bucknum = ((int64_t) key) % table->numbuckets;
	bucket_t *buck;	

	buck = table->bucket[bucknum];

	if (buck->keystring == key)
	{
		table->bucket[bucknum] = buck->next;
		return;
	}


	while(buck->next)
	{
		if (buck->next->keystring == key)
		{
			buck->next = buck->next->next;
			return;
		}

		buck = buck->next;
	}
	return;
}

#if 0
void Hash_BucketStats(hashtable_t *table)
{
	int i;
	bucket_t *buck;	
	int bucket_count;

	for (i = 0; i < table->numbuckets; i++) {
		bucket_count = 0;
		for (buck = table->bucket[i]; buck; buck = buck->next) {
			bucket_count++;
		}
		printf("table[%d] = %d\n", i, bucket_count);
	}
}
#endif

#endif // WITH_FTE_VFS
