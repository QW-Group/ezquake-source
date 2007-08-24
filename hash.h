//=============================
//David's hash tables
//string based.

#define Hash_BytesForBuckets(b) (sizeof(bucket_t)*b)

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc
typedef struct bucket_s {
	void *data;
	char *keystring;
	struct bucket_s *next;
} bucket_t;
typedef struct hashtable_s {
	int numbuckets;
	bucket_t **bucket;
} hashtable_t;

void Hash_InitTable(hashtable_t *table, int numbucks, void *mem);	//mem must be 0 filled. (memset(mem, 0, size))
int Hash_Key(char *name, int modulus);
void *Hash_Get(hashtable_t *table, char *name);
void *Hash_GetInsensative(hashtable_t *table, char *name);
void *Hash_GetKey(hashtable_t *table, int key);
void *Hash_GetNext(hashtable_t *table, char *name, void *old);
void *Hash_GetNextInsensative(hashtable_t *table, char *name, void *old);
void *Hash_Add(hashtable_t *table, char *name, void *data, bucket_t *buck);
void *Hash_AddInsensative(hashtable_t *table, char *name, void *data, bucket_t *buck);
void Hash_Remove(hashtable_t *table, char *name);
void Hash_RemoveData(hashtable_t *table, char *name, void *data);
void Hash_RemoveKey(hashtable_t *table, int key);
void *Hash_AddKey(hashtable_t *table, int key, void *data, bucket_t *buck);
