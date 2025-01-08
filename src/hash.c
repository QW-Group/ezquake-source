/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef SERVERONLY

#include "qwsvdef.h"

#else

#include <stdio.h> // <-- only needed for Hash_BucketStats
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "quakedef.h"
#include "q_shared.h"
#include "hash.h"

#endif

hashtable_t *Hash_InitTable(int numbucks)
{
	hashtable_t *table;

	table = Q_malloc(sizeof(*table));

	table->bucket = (bucket_t**)Q_calloc(numbucks, sizeof(bucket_t *));
	table->numbuckets = numbucks;

	return table;
}

void Hash_ShutdownTable(hashtable_t* table)
{
	int i;

	if (!table) {
		return;
	}

	for (i = 0; i < table->numbuckets; ++i) {
		bucket_t* list = table->bucket[i];

		while (list) {
			bucket_t* next = list->next;
			if (list->flags & HASH_BUCKET_KEYSTRING_OWNED) {
				Q_free(list->keystring);
			}
			Q_free(list);
			list = next;
		}
	}

	Q_free(table->bucket);
	Q_free(table);
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

int Hash_KeyInsensitive(const char *name, int modulus) {
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
int Hash_KeyInsensitive(char *name, int modulus)
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

void *Hash_GetInsensitive(hashtable_t *table, const char *name)
{
	int bucknum = Hash_KeyInsensitive(name, table->numbuckets);
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
	int bucknum = ((uintptr_t) key) % table->numbuckets;
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

void *Hash_GetNextInsensitive(hashtable_t *table, char *name, void *old)
{
	int bucknum = Hash_KeyInsensitive(name, table->numbuckets);
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

void *Hash_Add(hashtable_t *table, char *name, void *data) 
{
	int bucknum = Hash_Key(name, table->numbuckets);

	bucket_t *buck = (bucket_t *) Q_malloc(sizeof(bucket_t));
	char *keystring = Q_strdup(name);

	buck->data = data;
	buck->keystring = keystring;
	buck->next = table->bucket[bucknum];
	buck->flags = HASH_BUCKET_KEYSTRING_OWNED;
	table->bucket[bucknum] = buck;

	return buck;
}

void *Hash_AddInsensitive(hashtable_t *table, char *name, void *data) 
{
	int bucknum = Hash_KeyInsensitive(name, table->numbuckets);

	bucket_t *buck = (bucket_t *) Q_malloc(sizeof(bucket_t));
	char *keystring = Q_strdup(name);

	buck->data = data;
	buck->keystring = keystring;
	buck->next = table->bucket[bucknum];
	buck->flags = HASH_BUCKET_KEYSTRING_OWNED;
	table->bucket[bucknum] = buck;

	return buck;
}

void *Hash_AddKey(hashtable_t *table, char *key, void *data, bucket_t *buck)
{
	int bucknum = ((uintptr_t) key) % table->numbuckets;

	buck->data = data;
	buck->keystring = key;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void Hash_Remove(hashtable_t *table, char *name)
{
	int bucknum = Hash_Key(name, table->numbuckets);
	bucket_t *buck, *to_remove;

	buck = table->bucket[bucknum];

	if (!STRCMP(name, buck->keystring))
	{
		table->bucket[bucknum] = buck->next;
		Q_free(buck->keystring);
		Q_free(buck);
		return;
	}


	while(buck->next)
	{
		if (!STRCMP(name, buck->next->keystring))
		{
			to_remove = buck->next;
			buck->next = to_remove->next;
			Q_free(to_remove->keystring);
			Q_free(to_remove);
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
			Q_free(buck->keystring);
			Q_free(buck);
			return;
		}


	while(buck->next)
	{
		if (buck->next->data == data)
			if (!STRCMP(name, buck->next->keystring))
			{
				buck->next = buck->next->next;
				Q_free(buck->next->keystring);
				Q_free(buck->next);
				return;
			}

		buck = buck->next;
	}
	return;
}

void Hash_RemoveKey(hashtable_t *table, char *key)
{
	int bucknum = ((uintptr_t) key) % table->numbuckets;
	bucket_t *buck, *to_remove;

	buck = table->bucket[bucknum];

	if (buck->keystring == key)
	{
		table->bucket[bucknum] = buck->next;
		Q_free(buck->keystring);
		Q_free(buck);
		return;
	}


	while(buck->next)
	{
		if (buck->next->keystring == key)
		{
			to_remove = buck->next;
			buck->next = to_remove->next;
			Q_free(to_remove->keystring);
			Q_free(to_remove);
			return;
		}

		buck = buck->next;
	}
	return;
}

void Hash_Flush(hashtable_t *table) 
{
	int i;
	for (i = 0; i < table->numbuckets; i++)
	{
		bucket_t *bucket, *next;
		bucket = table->bucket[i];
		table->bucket[i] = NULL;
		
		while (bucket)
		{
			next = bucket->next;
			Q_free(bucket->keystring);
			Q_free(bucket);
			bucket = next;
		}
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
