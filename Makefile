# dal carattere '#' fino a fine riga, il testo dentro il Makefile e`
# un commento
# flags per la compilazione
CFLAGS = -std=c89 -pedantic
LD_LIBS = -lpthread 
# target ovvero nome dell'eseguibile che si intende produrre
TARGET = master
# object files necessari per produrre l'eseguibile
OBJ    = utente.o nodo.o master.o

# Si sfrutta la regola implicita per la produzione dei file oggetto in
# $(OBJ)
#
# Le "variabili" del Makefile sono espanse con $(NOME_VAR). Quindi
# scrivere
#
# $(TARGET): $(OBJ)
#
# equivale a scrivere
#
# master: utente.o nodo.o
#
# ovvero a specificare che per produrre l'eseguibile "master"
# servono i due object files "utente.o" e "nodo.o"

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET) $(LD_LIBS)

# solitamente il target "all" e` presente
all: $(TARGET)

# con "make clean" solitamente si vogliono eliminare i prodotti della
# compilazione e ripristinare il contenuto originale
clean:
	rm -f *.o $(TARGET) *~

# il target run si usa talvolta per eseguire l'applicazione
run: $(TARGET)
	./$(TARGET)
