#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//typy

struct Date {
	char d;
	char m;
	int y;
};

struct IndexFileElement {
	int key;
	int page_number;
};
struct MainFileElement {
	int key;
	struct Date data;
	int overflow_pointer;
};

struct IndexFile {
	char *name;
	int index;
	int size; //number of elements
	struct IndexFileElement *buffer;
};
struct MainFile {
	char *name;
	int page; //number of page in buffer
	struct MainFileElement *buffer;
};
struct OverflowFile {
	char *name;
	int index;
	int size; //number of elements
	struct MainFileElement *buffer;
	int maxsize; //in pages
};

// sta³e
#define B 1024 //pojemnosc strony w bajtach
static int bi = B / sizeof(struct IndexFileElement); //wspó³czynnik blokowania sekcji indeksu
static int bf = B / sizeof(struct MainFileElement); //wspó³czynnik blokowania sekcji g³ównej
static float a = .5;
static float b = .125;

//zmiennse globalne

int reads = 0;
int writes = 0;
int N = 0;

//metody
struct Date getRandDate() {
	struct tm * tm;
	time_t t;
	struct Date date;
	t = rand();
	t *= 60 * 60 * 24;
	tm = gmtime(&t);
	date.d = tm->tm_mday;
	date.m = tm->tm_mon + 1;
	date.y = tm->tm_year + 1900;
	return date;
}

void reorganise(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile);

void indexFileInit(struct IndexFile * indexFile, char * name) {
	indexFile->index = -1;
	indexFile->name = name;
	indexFile->size = 0;
	FILE * fileStream;
	fileStream = fopen(indexFile->name, "wb");
	if (fileStream)
		fclose(fileStream);
	else
		printf("B³¹d przy probie skasowania pliku %s\n", indexFile->name);
	indexFile->buffer = (struct IndexFileElement *)calloc(bi, sizeof(struct IndexFileElement));
}
void saveIndexPage(struct IndexFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "r+b");
	if (fileStream) {
		int offset = (file->index - file->index % bi) * sizeof(struct IndexFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fwrite(file->buffer, sizeof(struct IndexFileElement), bi, fileStream);
		fclose(fileStream);
		writes++;
	}
	else
		printf("B³¹d przy probie zapisania pliku %s\n", file->name);
}
void loadIndexPage(struct IndexFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "rb");
	if (fileStream) {
		int offset = (file->index - file->index % bi) * sizeof(struct IndexFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fread(file->buffer, sizeof(struct IndexFileElement), bi, fileStream);
		fclose(fileStream);
		reads++;
	}
	else
		printf("B³¹d przy probie odczytania pliku %s\n", file->name);
}
void addIndex(struct IndexFile * file, struct IndexFileElement e) {
	if (file->index != file->size - 1) {
		printf("B³¹d: nie mo¿na teraz dodaæ indeksu\n");
		return;
	}
	if ((file->index + 1) % bi == 0) { //jesli potrzeba nowej strony
		if (file->index + 1!= 0)
			saveIndexPage(file);
		for (int i = 0; i < bi; i++) {
			file->buffer[i] = (struct IndexFileElement) { 0, 0 };
		}
	} 
	file->buffer[(++file->index) % bi] = e;
	file->size++;
}
struct IndexFileElement readIndex(struct IndexFile * file, int index) {
	if (index < file->index - file->index % bi || index >= file->index - file->index % bi + bi) { //jesli nie jest w aktualnie zaladowanym buforze
		file->index = index;
		loadIndexPage(file);
	}
	else
		file->index = index;
	return file->buffer[index % bi];
}
void updateIndex(struct IndexFile * file, int index, struct IndexFileElement ife) {
	if (index < file->index - file->index % bi || index >= file->index - file->index % bi + bi) { //jesli nie jest w aktualnie zaladowanym buforze
		file->index = index;
		loadIndexPage(file);
	}
	else
		file->index = index;
	file->buffer[index % bi] = ife;
	saveIndexPage(file);
	return;
}

void mainFileInit(struct MainFile * file, char * name) {
	file->page = -1;
	file->name = name;
	FILE * fileStream;
	fileStream = fopen(file->name, "wb");
	if (fileStream)
		fclose(fileStream);
	else
		printf("B³¹d przy probie utworzenia pustego pliku %s\n", file->name);
	file->buffer = (struct MainFileElement *)calloc(bf, sizeof(struct MainFileElement));
}
void saveMainPage(struct MainFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "r+b");
	if (fileStream) {
		int offset = file->page * bf * sizeof(struct MainFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fwrite(file->buffer, sizeof(struct MainFileElement), bf, fileStream);
		fclose(fileStream);
		writes++;
	}
	else
		printf("B³¹d przy probie zapisania pliku %s\n", file->name);
}
void loadMainPage(struct MainFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "rb");
	if (fileStream) {
		for (int i = 0; i < bf; i++)
			file->buffer[i] = (struct MainFileElement) { 0, { 0,0,0 }, 0 };
		int offset = file->page * bf * sizeof(struct MainFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fread(file->buffer, sizeof(struct MainFileElement), bf, fileStream);
		fclose(fileStream);
		reads++;
	}
	else
		printf("B³¹d przy probie odczytania pliku %s\n", file->name);
}
struct MainFileElement readMainPage(struct MainFile *file, int page, int index) {
	if (page != file->page) { //jesli nie jest w aktualnie zaladowanym buforze
		file->page = page;
		loadIndexPage(file);
	}
	return file->buffer[index];
}

void overflowFileInit(struct OverflowFile * file, char * name, int maxsize) {
	file->buffer = (struct MainFileElement *)calloc(bf, sizeof(struct MainFileElement));
	file->index = -1;
	file->maxsize = maxsize;
	file->name = name;
	file->size = 0;
	FILE * fileStream;
	fileStream = fopen(file->name, "wb");
	if (fileStream)
		fclose(fileStream);
	else
		printf("B³¹d przy probie utworzenia pustego pliku %s\n", file->name);
}
void saveOverflowPage(struct OverflowFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "r+b");
	if (fileStream) {
		int offset = (file->index - (file->index) % bf) * sizeof(struct MainFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fwrite(file->buffer, sizeof(struct MainFileElement), bf, fileStream);
		fclose(fileStream);
		writes++;
	}
	else
		printf("B³¹d przy probie zapisania pliku %s\n", file->name);
}
void loadOverflowPage(struct OverflowFile * file) {
	FILE * fileStream;
	fileStream = fopen(file->name, "rb");
	if (fileStream) {
		for (int i = 0; i < bf; i++)
			file->buffer[i] = (struct MainFileElement) { 0, { 0,0,0 }, 0 };
		int offset = (file->index - file->index % bf) * sizeof(struct MainFileElement);
		fseek(fileStream, offset, SEEK_SET);
		fread(file->buffer, sizeof(struct MainFileElement), bf, fileStream);
		fclose(fileStream);
		reads++;
	}
	else
		printf("B³¹d przy probie odczytania pliku %s\n", file->name);
}
struct MainFileElement readOverflowElement(struct OverflowFile *file, int index) {
	if (index - index%bf != file->index - (file->index) % bf) { //jesli nie jest w aktualnie zaladowanym buforze
		file->index = index;
		loadOverflowPage(file);
	}
	else
		file->index = index;
	return file->buffer[index % bf];
}

void addRecord(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile, int key, struct Date data) {
	N++;
	if (ifile->size == 0) { //jesli plik pusty
		addIndex(ifile, (struct IndexFileElement) { key, 0 });
		saveIndexPage(ifile);
		mfile->buffer[0] = (struct MainFileElement) { key, data, -1 };
		mfile->page = 0;
		saveMainPage(mfile);
	}
	else {
		struct IndexFileElement ife = readIndex(ifile, 0);
		if (key < ife.key) { // na sam poczatek
			struct Date tmp;
			updateIndex(ifile, 0, (struct IndexFileElement) { key, ife.page_number });
			if (mfile->page != 0) {
				mfile->page = 0;
				loadMainPage(mfile);
			}
			tmp = mfile->buffer[0].data;
			mfile->buffer[0].key = key;
			mfile->buffer[0].data = data;
			saveMainPage(mfile);
			addRecord(ifile, mfile, ofile, ife.key, tmp);
		}
		else {
			int page = ife.page_number;
			for (int i = 1; i < ifile->size; i++) {
				ife = readIndex(ifile, i);
				if (key < ife.key)
					break;
				page = ife.page_number;
			}
			//strona wybrana
			if (page != mfile->page) {
				mfile->page = page;
				loadMainPage(mfile);
			}
			if (mfile->buffer[bf - 1].key == 0) { //jesli jest miejsce
				for (int i = bf - 1; i > 0; i--) {
					if (mfile->buffer[i - 1].key < key && mfile->buffer[i - 1].key != 0) {
						mfile->buffer[i] = (struct MainFileElement) { key, data, -1 };
						saveMainPage(mfile);
						break;
					}
					else
						mfile->buffer[i] = mfile->buffer[i - 1];
				}
			}
			else {//do overflow
				for (int i =bf - 1; i >= 0; i--) {
					if (key > mfile->buffer[i].key) {
						//dodaje do tej listy
						if (mfile->buffer[i].overflow_pointer == -1) { //jesli pusta
							mfile->buffer[i].overflow_pointer = ofile->size;
							if (ofile->index - (ofile->index) % bf != ofile->size - (ofile->size) % bf) { //jesli poza buforem
								ofile->index = ofile->size++;
								loadOverflowPage(ofile);
							}
							else
								ofile->index = ofile->size++;
							ofile->buffer[(ofile->index) % bf] = (struct MainFileElement) { key, data, -1 };
							saveMainPage(mfile);
							saveOverflowPage(ofile);
							if (ofile->size == ofile->maxsize*bf) {
								reorganise(ifile,mfile,ofile);
							}
							break;
						}
						else {
							struct MainFileElement prev, next;
							prev = mfile->buffer[i];
							if (ofile->index - (ofile->index) % bf != prev.overflow_pointer - (prev.overflow_pointer) % bf) {
								ofile->index = prev.overflow_pointer;
								loadOverflowPage(ofile);
							}
							else {
								ofile->index = prev.overflow_pointer;
							}
							next = ofile->buffer[(prev.overflow_pointer) % bf];
							if (next.key > key) {//dodanie miedzy strona a of
								prev.data = data;
								prev.key = key;
								mfile->buffer[i].overflow_pointer = ofile->size;
								if (ofile->index - (ofile->index) % bf != ofile->size - (ofile->size) % bf) { //jesli poza buforem
									ofile->index = ofile->size++;
									loadOverflowPage(ofile);
								}
								else
									ofile->index = ofile->size++;
								ofile->buffer[(ofile->index) % bf] = prev;
								saveMainPage(mfile);
								saveOverflowPage(ofile);
								if (ofile->size == ofile->maxsize*bf) {
									reorganise(ifile, mfile, ofile);
								}
								break;
							}
							else { //dodanie gdzies w srodku
								int prev_index;
								while (next.key <= key && next.overflow_pointer != -1) {//dopoki key nie bedzie pomiedzy prev i next lub next bedzie ostatnie
									prev_index = prev.overflow_pointer;
									prev = next;
									if (ofile->index - (ofile->index) % bf != prev.overflow_pointer - (prev.overflow_pointer) % bf) {
										ofile->index = prev.overflow_pointer;
										loadOverflowPage(ofile);
									}
									else {
										ofile->index = prev.overflow_pointer;
									}
									next = ofile->buffer[(ofile->index) % bf];
								}
								//teraz dodac miedzy prev a next;
								if (next.overflow_pointer == -1 && key >= next.key ) { // na koniec listy
									ofile->buffer[(ofile->index) % bf].overflow_pointer = ofile->size;

									if (ofile->index - (ofile->index) % bf != ofile->size - (ofile->size) % bf) { //jesli poza buforem
										saveOverflowPage(ofile);
										ofile->index = ofile->size++;
										loadOverflowPage(ofile);
									}
									else
										ofile->index = ofile->size++;
									ofile->buffer[(ofile->index) % bf] = (struct MainFileElement) { key, data, -1 };
									saveMainPage(mfile);
									saveOverflowPage(ofile);
									if (ofile->size == ofile->maxsize*bf) {
										reorganise(ifile, mfile, ofile);
									}
									break;

								}
								else { // w srodek listy
									struct MainFileElement newE = { key, data, prev.overflow_pointer };
									if (ofile->index - (ofile->index) % bf != prev_index - prev_index % bf) {
										ofile->index = prev_index;
										loadOverflowPage(ofile);
									}
									else
										ofile->index = prev_index;
									ofile->buffer[(ofile->index) % bf].overflow_pointer = ofile->size; // poprawiam porzednika

									if (ofile->index - (ofile->index) % bf != ofile->size - (ofile->size) % bf) { //jesli poza buforem
										saveOverflowPage(ofile);
										ofile->index = ofile->size++;
										loadOverflowPage(ofile);
									}
									else
										ofile->index = ofile->size++;
									ofile->buffer[(ofile->index) % bf] = newE;
									saveMainPage(mfile);
									saveOverflowPage(ofile);
									if (ofile->size == ofile->maxsize*bf) {
										reorganise(ifile, mfile, ofile);
									}
									break;
								}
							}
						}

					}
				}
			}
		}
	}
}
struct MainFileElement getRecord(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile, int key) {
	if (ifile->size == 0) {
		return (struct MainFileElement) { 0, { 0, 0, 0 }, 0 };
	}
	struct IndexFileElement ife = readIndex(ifile, 0);
	if (ife.key > key) {
		return (struct MainFileElement) { 0, { 0, 0, 0 }, 0 };
	}
	int page = ife.page_number;
	for (int i = 0; i < ifile->size; i++) {
		ife = readIndex(ifile, i);
		if (ife.key > key)
			break;
		page = ife.page_number;
	}
	readMainPage(mfile, page, 0);
	for (int i = bf; i > 0; i--) {
		if (mfile->buffer[i - 1].key == 0)
			continue;
		if (mfile->buffer[i - 1].key == key) {
			return mfile->buffer[i - 1];
		}
		else if (mfile->buffer[i - 1].key < key) {
			struct MainFileElement mfe;
			mfe = mfile->buffer[i - 1];
			while (mfe.overflow_pointer != -1) {
				mfe = readOverflowElement(ofile, mfe.overflow_pointer);
				if (mfe.key == key)
					return mfe;
				if (mfe.key > key)
					break;
			}
			return (struct MainFileElement) { 0, { 0, 0, 0 }, 0 };
		}
	}

}
void revievFile(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile) {
	for (int i = 0; i < ifile->size; i++) {
		int page = readIndex(ifile, i).page_number;
		readMainPage(mfile, page, 0);
		for (int j = 0; j < bf; j++) {
			if (mfile->buffer[j].key == 0)
				break;
			printf("%d %d %d %d %d\n", mfile->buffer[j].key, mfile->buffer[j].data.d, mfile->buffer[j].data.m, mfile->buffer[j].data.y, mfile->buffer[j].overflow_pointer);
			if (mfile->buffer[j].overflow_pointer != -1) {
				struct MainFileElement node;
				node = readOverflowElement(ofile, mfile->buffer[j].overflow_pointer);
				printf("%d %d %d %d %d\n", node.key, node.data.d, node.data.m, node.data.y, node.overflow_pointer);
				while(node.overflow_pointer != -1) {
					node = readOverflowElement(ofile, node.overflow_pointer);
					printf("%d %d %d %d %d\n", node.key, node.data.d, node.data.m, node.data.y, node.overflow_pointer);
				}
			}
		}
	}
}
void manage(struct IndexFile * ifile, struct MainFile * mfile, int key, struct Date data, int page_place) {
	if (page_place == 0) {
		addIndex(ifile, (struct IndexFileElement) { key, page_place });
		saveMainPage(mfile);
		mfile->page++;
		for (int i = 0; i < bf; i++) {
			mfile->buffer[i] = (struct MainFileElement) { 0, { 0,0,0 }, 0 };
		}
	}
	mfile->buffer[page_place] = (struct MainFileElement) { key, data, -1 };
	
}
void reorganise(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile) {
	struct IndexFile indexFile;
	struct MainFile mainFile;
	struct OverflowFile overflowFile;
	saveIndexPage(ifile);
	saveMainPage(mfile);
	saveOverflowPage(ofile);
	int page_place = 0;
	int no_records = 0;
	indexFileInit(&indexFile, "index2.file");
	mainFileInit(&mainFile, "main2.file");
	for (int i = 0; i < ifile->size; i++) {
		int page = readIndex(ifile, i).page_number;
		readMainPage(mfile, page, 0);
		for (int j = 0; j < bf; j++) {
			if (mfile->buffer[j].key == 0)
				break;
			manage(&indexFile, &mainFile, mfile->buffer[j].key, mfile->buffer[j].data, page_place);
			no_records++;
			if (page_place + 1 >= a*bf) 
				page_place = 0;
			else 
				++page_place;
			if (mfile->buffer[j].overflow_pointer != -1) {
				struct MainFileElement node;
				node = readOverflowElement(ofile, mfile->buffer[j].overflow_pointer);
				manage(&indexFile, &mainFile, node.key, node.data, page_place);
				no_records++;
				if (page_place + 1 >= a*bf) 
					page_place = 0;
				else 
					++page_place;
				while (node.overflow_pointer != -1) {
					node = readOverflowElement(ofile, node.overflow_pointer);
					manage(&indexFile, &mainFile, node.key, node.data, page_place);
					no_records++;
					if (page_place + 1 >= a*bf)
						page_place = 0;
					else 
						++page_place;
				}
			}
		}
	}
	saveMainPage(&mainFile);
	saveIndexPage(&indexFile);
	overflowFileInit(&overflowFile, "overflow2.file", no_records * b + 1);
	remove(ifile->name);
	remove(mfile->name);
	remove(ofile->name);
	rename(indexFile.name, ifile->name);
	rename(mainFile.name, mfile->name);
	rename(overflowFile.name, ofile->name);
	ifile->buffer = indexFile.buffer;
	ifile->index = indexFile.index;
	ifile->size = indexFile.size;
	mfile->buffer = mainFile.buffer;
	mfile->page = mainFile.page;

	ofile->buffer = overflowFile.buffer;
	ofile->index = overflowFile.index;
	ofile->maxsize = overflowFile.maxsize;
	ofile->size = overflowFile.size;
}
void printAll(struct IndexFile * ifile, struct MainFile * mfile, struct OverflowFile * ofile) {
	FILE * filestream;
	struct IndexFileElement ife;
	struct MainFileElement mfe;
	printf("Index:\n");
	filestream = fopen(ifile->name, "r+b");
	fflush(filestream);
	for (int i = 0; i < ifile->size; i++) {
		fseek(filestream, i * sizeof(struct IndexFileElement), SEEK_SET);
		fread(&ife, sizeof(struct IndexFileElement), 1, filestream);
		printf("%d\t%d\n", ife.key, ife.page_number);
	}
	fclose(filestream);

	printf("\nPrimary area:\n");
	filestream = fopen(mfile->name, "r+b");
	fflush(filestream);
	for (int i = 0; i < ifile->size; i++) {
		for (int j = 0; j < bf; j++) {
			fseek(filestream, (i*bf + j) * sizeof(struct MainFileElement), SEEK_SET);
			fread(&mfe, sizeof(struct MainFileElement), 1, filestream);
			printf("%d\t%d\t%d\t%d\t%d\n", mfe.key, mfe.data.d, mfe.data.m, mfe.data.y, mfe.overflow_pointer);
		}
	}
	fclose(filestream);

	printf("\nOveflow area:\n");
	filestream = fopen(ofile->name, "r+b");
	fflush(filestream);
	for (int i = 0; i < ofile->size; i++) {
		fseek(filestream, i * sizeof(struct MainFileElement), SEEK_SET);
		fread(&mfe, sizeof(struct MainFileElement), 1, filestream);
		printf("%d\t%d\t%d\t%d\t%d\n", mfe.key, mfe.data.d, mfe.data.m, mfe.data.y, mfe.overflow_pointer);
	}
	fclose(filestream);
	printf("\n");
}

int main() {
	struct IndexFile indexFile;
	struct MainFile mainFile;
	struct OverflowFile overflowFile;
	char c;
	indexFileInit(&indexFile, "index.file");
	mainFileInit(&mainFile, "main.file");
	overflowFileInit(&overflowFile, "overflow.file", 1);

	srand(time(NULL));
	for (int i = 1; i <= 8000; i++) {
		int x = abs(rand()) + 1;
		addRecord(&indexFile, &mainFile, &overflowFile, x, getRandDate());
		if (i == 500 || i == 1000 || i == 2000 || i == 4000 || i == 8000) {
			double y;
			y = indexFile.size*(sizeof(struct IndexFileElement) + bf * sizeof(struct MainFileElement)) + overflowFile.size * sizeof(struct MainFileElement);
			y /= i;
			printf("%f\n", y);
		}
	}
	
	while (scanf("%c", &c)) {
		if (c == 'i') {
			int x = abs(rand()) + 1;
			addRecord(&indexFile, &mainFile, &overflowFile, x, getRandDate());
			printf("%d\n", x);
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
		else if (c == 'I') {
			struct Date date;
			int k;
			scanf("%d %d %d %d", &k, &(date.d), &(date.m), &(date.y));
			addRecord(&indexFile, &mainFile, &overflowFile, k, date);
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
		else if (c == 'r') {
			int x;
			scanf("%d", &x);
			struct MainFileElement mfe = getRecord(&indexFile, &mainFile, &overflowFile, x);
			if (mfe.data.d == 0)
				printf("Nie ma takiego rekordu\n");
			else {
				printf("%d %d %d %d %d\n", mfe.key, mfe.data.d, mfe.data.m, mfe.data.y, mfe.overflow_pointer);
			}
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
		else if (c == 'R') {
			revievFile(&indexFile, &mainFile, &overflowFile);
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
		else if (c == 'f') {
			reorganise(&indexFile, &mainFile, &overflowFile);
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
		else if (c == 'p') {
			printAll(&indexFile, &mainFile, &overflowFile);
			printf("wirtes:\t%d\nreads:\t%d\n\n", writes, reads);
		}
	}

	return 0;


}