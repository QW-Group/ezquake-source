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
//=============================
//David's hash tables
//string based.

#ifndef __HASH_H__
#define __HASH_H__

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc
typedef struct bucket_s {
	void *data;
	char *keystring;
	struct bucket_s *next;
	int flags;
} bucket_t;

#define HASH_BUCKET_KEYSTRING_OWNED 1

typedef struct hashtable_s {
	int numbuckets;
	bucket_t **bucket;
} hashtable_t;

hashtable_t *Hash_InitTable(int numbucks);
int Hash_Key(char *name, int modulus);
void *Hash_Get(hashtable_t *table, char *name);
void *Hash_GetInsensitive(hashtable_t *table, const char *name);
void *Hash_GetKey(hashtable_t *table, char *key);
void *Hash_GetNext(hashtable_t *table, char *name, void *old);
void *Hash_GetNextInsensitive(hashtable_t *table, char *name, void *old);
void *Hash_Add(hashtable_t *table, char *name, void *data); 
void *Hash_AddInsensitive(hashtable_t *table, char *name, void *data); 
void Hash_Remove(hashtable_t *table, char *name);
void Hash_RemoveData(hashtable_t *table, char *name, void *data);
void Hash_RemoveKey(hashtable_t *table, char *key);
void *Hash_AddKey(hashtable_t *table, char *key, void *data, bucket_t *buck);
void Hash_Flush(hashtable_t *table);

#if 0
/* Print some stats on the bucket distrubution */
void Hash_BucketStats(hashtable_t *table);
#endif

#endif // __HASH_H__
