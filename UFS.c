#include "UFS.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disque.h"

// Quelques fonctions qui pourraient vous être utiles
int NumberofDirEntry(int Size) {
	return Size/sizeof(DirEntry);
}

int min(int a, int b) {
	return a<b ? a : b;
}

int max(int a, int b) {
	return a>b ? a : b;
}

/* Cette fonction va extraire le repertoire d'une chemin d'acces complet, et le copier
   dans pDir.  Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pDir le string "/doc/tmp" . Si le chemin fourni est pPath="/a.txt", la fonction
   va retourner pDir="/". Si le string fourni est pPath="/", cette fonction va retourner pDir="/".
   Cette fonction est calquée sur dirname, que je ne conseille pas d'utiliser car elle fait appel
   à des variables statiques/modifie le string entrant. */
int GetDirFromPath(const char *pPath, char *pDir) {
	strcpy(pDir,pPath);
	int len = strlen(pDir); // length, EXCLUDING null
	int index;

	// On va a reculons, de la fin au debut
	while (pDir[len]!='/') {
		len--;
		if (len <0) {
			// Il n'y avait pas de slash dans le pathname
			return 0;
		}
	}
	if (len==0) {
		// Le fichier se trouve dans le root!
		pDir[0] = '/';
		pDir[1] = 0;
	}
	else {
		// On remplace le slash par une fin de chaine de caractere
		pDir[len] = '\0';
	}
	return 1;
}

/* Cette fonction va extraire le nom de fichier d'une chemin d'acces complet.
   Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pFilename le string "a.txt" . La fonction retourne 1 si elle
   a trouvée le nom de fichier avec succes, et 0 autrement. */
int GetFilenameFromPath(const char *pPath, char *pFilename) {
	// Pour extraire le nom de fichier d'un path complet
	char *pStrippedFilename = strrchr(pPath,'/');
	if (pStrippedFilename!=NULL) {
		++pStrippedFilename; // On avance pour passer le slash
		if ((*pStrippedFilename) != '\0') {
			// On copie le nom de fichier trouve
			strcpy(pFilename, pStrippedFilename);
			return 1;
		}
	}
	return 0;
}


/* Cette fonction sert à afficher à l'écran le contenu d'une structure d'i-node */
void printiNode(iNodeEntry iNode) {
	printf("\t\t========= inode %d ===========\n",iNode.iNodeStat.st_ino);
	printf("\t\t  blocks:%d\n",iNode.iNodeStat.st_blocks);
	printf("\t\t  size:%d\n",iNode.iNodeStat.st_size);
	printf("\t\t  mode:0x%x\n",iNode.iNodeStat.st_mode);
	int index = 0;
	for (index =0; index < N_BLOCK_PER_INODE; index++) {
		printf("\t\t      Block[%d]=%d\n",index,iNode.Block[index]);
	}
}


/* ----------------------------------------------------------------------------------------
					            à vous de jouer, maintenant!
   ---------------------------------------------------------------------------------------- */
// Choisir un inode dans la bitmap
static int getFirstFreeInode(){
    char data[BLOCK_SIZE];
    int errReadBlock;
    
    // On vérifie le retour de la fonction readBlock
    errReadBlock = ReadBlock(FREE_INODE_BITMAP, data);
    if (errReadBlock == 0 || errReadBlock == -1) return -1;
    
    // On itère sur le bitmap d'iNodes, et quand on en trouve un de libre, on inscrit sa valeur à 0 dans le bitmap et on inscrit le bloc
    for (int i = ROOT_INODE; i < N_INODE_ON_DISK; i++){
        if (data[i] != 0){
            printf("GLOFS: Saisie i-node %d\n", i);
            data[i] = 0;
            WriteBlock(FREE_INODE_BITMAP, data);
            return i;
        }
    }
    return -1;
}

// Choisir un bloc dans la bitmap
static int getFirstFreeBlock(){
    char data[BLOCK_SIZE];
    int errReadBlock;
    
    // On vérifie le retour de la fonction readBlock
    errReadBlock = ReadBlock(FREE_BLOCK_BITMAP, data);
    if (errReadBlock == 0 || errReadBlock == -1) return -1;
    
    // On itère sur le bitmap de blocs, et quand on en trouve un de libre, on inscrit sa valeur à 0 dans le bitmap et on inscrit le bloc
    for (int i = 0; i < N_BLOCK_ON_DISK; i++){
        if (data[i] != 0){
            printf("GLOFS: Saisie bloc %d\n", i);
            data[i] = 0;
            WriteBlock(FREE_BLOCK_BITMAP, data);
            return i;
        }
    }
    return -1;
}

// Écrire l'inode dans le bloc des inodes
static int addiNodeToiNodeBlock(iNodeEntry *iNode){
    char data[BLOCK_SIZE];
    
    // Trouver l'offset dans les blocs pour un inode particulier
    ino iNodeNumber = iNode->iNodeStat.st_ino;
    int iNodeBlockNum = BASE_BLOCK_INODE + iNodeNumber/NUM_INODE_PER_BLOCK;
    int iNodeOffset = iNodeNumber % NUM_INODE_PER_BLOCK;
    
    // On charge le bloc qui contient l'inode dans data
    int retRead = ReadBlock(iNodeBlockNum, data);
    
    // Vérification de la fonctio readBlock
    if (retRead == 0 || retRead == -1){
        return -1;
    }
    
    iNodeEntry *entries = (iNodeEntry*) data;
    
    // Écriture de l'inode sur le bloc, puis sauvegarde en mémoire
    entries[iNodeOffset] = *iNode;
    WriteBlock(iNodeBlockNum, data);
    
    return 0;
}

// Copie un inode dans entry selon un numéro d'inode
static int getInode(int inodeNumber, iNodeEntry **entry){
    char data[BLOCK_SIZE];
    
    // Trouver l'offset dans les blocs pour un inode particulier
    int iNodeBlockNum = BASE_BLOCK_INODE + inodeNumber/NUM_INODE_PER_BLOCK;
    int iNodeOffset = inodeNumber % NUM_INODE_PER_BLOCK;
    
    // On charge le bloc qui contient l'inode dans data
    int retRead = ReadBlock(iNodeBlockNum, data);
    
    // Vérification de la fonctio readBlock
    if (retRead == 0 || retRead == -1){
        return -1;
    }
    
    // Copie les stats de l'inode
    iNodeEntry *inode = (iNodeEntry*) data + iNodeOffset;
    (*entry)->iNodeStat = inode->iNodeStat;
    // Copie le block de l'inode
    for (int i = 0; i < N_BLOCK_PER_INODE; ++i) {
        (*entry)->Block[i] = inode->Block[i];
    }
    
    return 0;
}

// Enlever l'inode de la bitmap
static int releaseInode(int numInode){
    char data[BLOCK_SIZE];
    ReadBlock(FREE_INODE_BITMAP, data);
    data[numInode] = 1;
    printf("GLOFS: Relache i-node %d\n", numInode);
    WriteBlock(FREE_INODE_BITMAP, data);
    
    iNodeEntry* inode = alloca(sizeof(inode));
    getInode(numInode, &inode);
    
    inode->iNodeStat.st_blocks = 0;
    inode->iNodeStat.st_ino = numInode;
    inode->iNodeStat.st_mode &= ~G_IRWXG;
    inode->iNodeStat.st_mode &= ~G_IRWXU;
    inode->iNodeStat.st_mode &= ~G_IFDIR;
    inode->iNodeStat.st_mode &= ~G_IFREG;
    inode->iNodeStat.st_mode &= ~G_IFLNK;
    inode->iNodeStat.st_nlink = 0;
    inode->iNodeStat.st_size = 0;
    
    addiNodeToiNodeBlock(inode);
    
    return 0;
}

// Enlever le bloc de la bitmap
static int releaseBlock(int numBlock){
    char data[BLOCK_SIZE];
    ReadBlock(FREE_BLOCK_BITMAP, data);
    data[numBlock] = 1;
    printf("GLOFS: Relache bloc %d\n", numBlock);
    WriteBlock(FREE_BLOCK_BITMAP, data);
    return 0;
}

// Fonction pour trouver le numéro d'inode d'un fichier particulier
static int getFileInodeNumber(const char *filename, int parentInodeNumber){
    // Vérifie si on passe '/' (pour les fonctions suivantes) comme nom de fichier ou un nom de fichier vide
    if (strcmp(filename, "") == 0) {
        return parentInodeNumber;
    }
    
    // On va chercher le numéro de bloc associé à l'inode, puis l'offset dans le bloc
    char data[BLOCK_SIZE];
    int iNodeBlockNum = BASE_BLOCK_INODE + parentInodeNumber/NUM_INODE_PER_BLOCK;
    int iNodeOffset = parentInodeNumber % NUM_INODE_PER_BLOCK;
    
    // On charge le bloc qui contient l'inode dans data
    int retRead = ReadBlock(iNodeBlockNum, data);
    
    // Vérification de la fonctio readBlock
    if (retRead == 0 || retRead == -1){
        return -1;
    }
    iNodeEntry *inodes = (iNodeEntry*) data;
    
    // Avant de changer le contenu de data, on vérifie combien de fichiers sont associés à l'inode
    int qtDir = NumberofDirEntry(inodes[iNodeOffset].iNodeStat.st_size);
    
    // On charge le contenu du bloc pointé par l'inode dans data
    retRead = ReadBlock(inodes[iNodeOffset].Block[0], data);
    
    // Vérification de la fonctio readBlock
    if (retRead == 0 || retRead == -1){
        return -1;
    }
    
    DirEntry *entries = (DirEntry*) data;

    //On itère sur les fichiers contenu dans le directory (bloc) et on vérifie si on trouve une correspondance avec le filename
    for (int i = 0; i < qtDir; i++){
        if (strcmp(entries[i].Filename, filename) == 0){
            return entries[i].iNode;
        }
    }
    
    // On n'a pas trouvé le fichier
    return -1;
}

// Retourne le numéro d'inode d'un fichier avec son path
static int getInodeFromPath(const char *path){
    // On assigne que le premier inode est le root
    int lastInode = ROOT_INODE;
    
    // On sépare la path pour chaque file selon les '/'
    char* files;
    char pathCopy[strlen(path)];
    strcpy(pathCopy, path);
    files = strtok (pathCopy,"/");
    
    // On itère sur chaque élément de la path, donc sur les files
    // code tiré de http://www.cplusplus.com/reference/cstring/strtok/
    while (files != NULL)
    {
        lastInode = getFileInodeNumber(files, lastInode);
        files = strtok (NULL, "/");
        
    }
    
    // On retourne le dernier inode visité, soit au bout du path
    return lastInode;
     
}

// ------------------------------------------------------------------------

int bd_countfreeblocks(void) {
    int nbrFreeBlock = 0;
    char data[BLOCK_SIZE];
    int errReadBlock;
    
    // On assigne une valeur de retour à la fonction ReadBLock
    errReadBlock = ReadBlock(FREE_BLOCK_BITMAP, data);

    // On vérifie la valeur du ReadBlock
    if (errReadBlock == 0 || errReadBlock == -1){
        return errReadBlock;
    }
    
    // On itère sur les blocs dans la bitmap des blocs
    for (int i = 0; i < N_BLOCK_ON_DISK; i++){
        if (data[i] !=0){
            nbrFreeBlock++;
        }
    }
    return nbrFreeBlock;
}

int bd_stat(const char *pFilename, gstat *pStat) {
    int inodeNum = getInodeFromPath(pFilename);
    
    // On regarde si l'iNode existe
    if (inodeNum == -1) return -1;
    
    // On va chercher l'iNode
    iNodeEntry* inode = alloca(sizeof(*inode));
    getInode(inodeNum, &inode);
    
    // On écrit les valeurs dans pStat
    pStat->st_blocks = inode->iNodeStat.st_blocks;
    pStat->st_ino = inode->iNodeStat.st_ino;
    pStat->st_mode = inode->iNodeStat.st_mode;
    pStat->st_nlink = inode->iNodeStat.st_nlink;
    pStat->st_size = inode->iNodeStat.st_size;
    
    return 0;
}

int bd_create(const char *pFilename) {
    char parentFilename[strlen(pFilename)];
    char trunkatedFilename[FILENAME_SIZE];
    GetDirFromPath(pFilename, parentFilename);
    
    // Aller chercher les numéros des inodes
    int inodeNumPathParent = getInodeFromPath(parentFilename);
    int inodeNumFilename = getInodeFromPath(pFilename);
    
    // On va chercher le inode du fichier parent de pFilename
    iNodeEntry* inodePathParent = alloca(sizeof(*inodePathParent));
    getInode(inodeNumPathParent, &inodePathParent);
    
    // Vérifier que le répertoire existe
    if (inodeNumPathParent == -1) return -1;
    
    //Vérifier que le fichier n'existe pas déjà
    if (inodeNumFilename != -1) return -2;
    
    // Vérifier que le répertoire n'est pas déjà plein
    if(inodePathParent->iNodeStat.st_size >= BLOCK_SIZE) return -4;
    
    
    // Aller chercher un numéro d'iNode libre, puis le iNode associé à ce numéro
    int freeInodeNumber = getFirstFreeInode();
    
    // Si pu d'iNode disponible
    if(freeInodeNumber == -1)return -5;
    
    iNodeEntry *iNode = alloca(sizeof(iNode));
    getInode(freeInodeNumber, &iNode);
    
    // Initialisation du iNode selon les params voulus
    iNode->iNodeStat.st_mode = 0;
    iNode->iNodeStat.st_mode |= G_IFREG;
    iNode->iNodeStat.st_mode |= G_IRWXG;
    iNode->iNodeStat.st_mode |= G_IRWXU;
    iNode->iNodeStat.st_size = 0;
    iNode->iNodeStat.st_blocks = 0;
    iNode->iNodeStat.st_nlink = 1;
    iNode->iNodeStat.st_ino = freeInodeNumber;
    
    // On écrit le iNode en mémoire
    addiNodeToiNodeBlock(iNode);
    
    
    char data[BLOCK_SIZE];
    // Test du readblock
    int errReadBlock = ReadBlock(inodePathParent->Block[0], data);
    if(errReadBlock == 0 || errReadBlock == -1) return -1;
    
    // On regarde la quantité de fichiers présents dans le iNode du parent
    DirEntry *dirData = (DirEntry*) data;
    int qtDir = NumberofDirEntry(inodePathParent->iNodeStat.st_size);
    GetFilenameFromPath(pFilename, trunkatedFilename);
    
    // On ajoute le nom et le iNode
    strcpy(dirData[qtDir].Filename, trunkatedFilename);
    dirData[qtDir].iNode = freeInodeNumber;
    
    // Incrémenter la grosseur de size dans l'inode
    inodePathParent->iNodeStat.st_size += sizeof(DirEntry);
    addiNodeToiNodeBlock(inodePathParent);
    
    // On sauvegarde le bloc écrit
    WriteBlock(inodePathParent->Block[0], data);
    return 0;
}

int bd_read(const char *pFilename, char *buffer, int offset, int numbytes) {
	int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    
    getInode(iNodeNumFilename, &iNodeFilename);
    
    // On vérifie que le fichier existe
    if(iNodeNumFilename == -1){
        return -1;
    }
    // On vérifie que le fichier n'est pas un répertoire
    if(iNodeFilename->iNodeStat.st_mode & G_IFDIR){
        return -2;
    }

    // On regarde si le bloc est vide
    if(iNodeFilename->iNodeStat.st_blocks == 0){
        return 0;
    }
    
    char data[BLOCK_SIZE];
    ReadBlock(iNodeFilename->Block[0], data);
    
    int i = 0;
    
    // On lit le contenu des données et on l'écrit dans le buffer
    while (i < numbytes && offset < iNodeFilename->iNodeStat.st_size){
        buffer[i] = data[offset];
        i++;
        offset++;
    }
    
    return i;
}

int bd_mkdir(const char *pDirName) {
    char parentDir[strlen(pDirName)];
    GetDirFromPath(pDirName, parentDir);
    
    int parentiNodeNum = getInodeFromPath(parentDir);
    
    // On regarde si le directory du parent existe
    if(parentiNodeNum == -1) return -1;
    
    int diriNode = getInodeFromPath(pDirName);
    
    // On regarde si le fichier existe déjà
    if (diriNode != -1) return -2;
    
    char fileName[FILENAME_SIZE];
    GetFilenameFromPath(pDirName, fileName);
    
    // On regarde si le nom du fichier n'est pas vide
    if (strcmp(fileName, "") == 0)return -3;
    
    // On regarde si le fichier n'est pas plein déjà
    iNodeEntry* parentiNode = alloca(sizeof(*parentiNode));
    getInode(parentiNodeNum, &parentiNode);
    if (parentiNode->iNodeStat.st_size >= BLOCK_SIZE)return -4;
    
    int reservediNodeNum = getFirstFreeInode();
    iNodeEntry* newiNode = alloca(sizeof(*newiNode));
    
    getInode(reservediNodeNum, &newiNode);
    
    // On met les bonnes valeurs dans l'iNode
    newiNode->iNodeStat.st_mode = 0;
    newiNode->iNodeStat.st_mode |= G_IFDIR;
    newiNode->iNodeStat.st_mode |= G_IRWXG;
    newiNode->iNodeStat.st_mode |= G_IRWXU;
    newiNode->iNodeStat.st_ino = reservediNodeNum;
    newiNode->iNodeStat.st_nlink = 2;
    newiNode->iNodeStat.st_size = 2 * sizeof(DirEntry);
    newiNode->iNodeStat.st_blocks = 1;
    
    int freeBlockNum = getFirstFreeBlock();
    newiNode->Block[0] = freeBlockNum;
    addiNodeToiNodeBlock(newiNode);
    
    
    char data[BLOCK_SIZE];
    // On va chercher le bloc associé au courrant
    ReadBlock(newiNode->Block[0], data);
    DirEntry* entries = (DirEntry*) data;
    
    int numEntry = 0;
    
    // On met le numéro d'inode de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    entries[numEntry].iNode = reservediNodeNum;
    strcpy(entries[numEntry++].Filename, ".");
    
    entries[numEntry].iNode = parentiNodeNum;
    strcpy(entries[numEntry].Filename, "..");
    
    // On écrit le bloc modifié sur le disque
    WriteBlock(newiNode->Block[0], data);
    
    // On va chercher le bloc associé au parent
    ReadBlock(parentiNode->Block[0], data);
    entries = (DirEntry*) data;
    
    // On trouve le premier endroit disponible pour mettre le hardlink
    numEntry = NumberofDirEntry(parentiNode->iNodeStat.st_size);
    
    //On incrémente la grosseur du dossier parent du nouveau lien et le nombre de liens de pPathExistant
    parentiNode->iNodeStat.st_size += sizeof(DirEntry);
    parentiNode->iNodeStat.st_nlink++;
    
    // On met le numéro d'inode de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    entries[numEntry].iNode = reservediNodeNum;
    
    // On met le nom du fichier de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    char nomLien[FILENAME_SIZE];
    GetFilenameFromPath(pDirName, nomLien);
    strcpy(entries[numEntry].Filename, nomLien);
    
    // On écrit le bloc modifié sur le disque
    WriteBlock(parentiNode->Block[0], data);
    
    // Écriture des inodes
    addiNodeToiNodeBlock(parentiNode);
    
    return 0;

}

int bd_write(const char *pFilename, const char *buffer, int offset, int numbytes) { 
    int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    
    getInode(iNodeNumFilename, &iNodeFilename);
    
    // On vérifie que le fichier existe
    if(iNodeNumFilename == -1){
        return -1;
    }
    // On vérifie que le fichier n'est pas un répertoire
    if(iNodeFilename->iNodeStat.st_mode & G_IFDIR){
        return -2;
    }
    
    if(offset > iNodeFilename->iNodeStat.st_size){
        return -3;
    }
    
    // On vérifie que l'offset n'est pas plus grand que la taille maximale d'un fichier
    if(offset > MAX_FILE_SIZE){
        return -4;
    }
    
    if(iNodeFilename->iNodeStat.st_blocks == 0 && iNodeFilename->iNodeStat.st_size == 0 && numbytes > 0) {
        iNodeFilename->Block[0] = getFirstFreeBlock();
        iNodeFilename->iNodeStat.st_blocks++;
    }
    
    char data[BLOCK_SIZE];
    ReadBlock(iNodeFilename->Block[0], data);
    
    int i = 0;
    
    // On fait la lecture des données dans le fichier
    while (i < numbytes && iNodeFilename->iNodeStat.st_size < MAX_FILE_SIZE){
        data[offset] = buffer[i];
        i++;
        offset++;
        if (i > iNodeFilename->iNodeStat.st_size || offset > iNodeFilename->iNodeStat.st_size){
            iNodeFilename->iNodeStat.st_size++;
        }
    }
    
    // On écrit l'iNode et le bloc
    addiNodeToiNodeBlock(iNodeFilename);
    WriteBlock(iNodeFilename->Block[0], data);
    return i;
}

int bd_hardlink(const char *pPathExistant, const char *pPathNouveauLien) {
    // On va chercher les numéros d'inodes de pPathExistant et de pPathNouveauLien
    int inodeNumPathExistant = getInodeFromPath(pPathExistant);
    int inodeNumPathNouveau = getInodeFromPath(pPathNouveauLien);
    
    // On va chercher l'inode correspondant à pPathExistant
    iNodeEntry* inodePathExistant  = alloca(sizeof(*inodePathExistant));
    getInode(inodeNumPathExistant, &inodePathExistant);
    
    // On va chercher le path du parent de pPathNouveauLien, ainsi que l'inode associé à celui-ci
    char pathParent[strlen(pPathNouveauLien)];
    GetDirFromPath(pPathNouveauLien, pathParent);
    iNodeEntry* inodePathParent = alloca(sizeof(*inodePathParent));
    int inodeNumParent = getInodeFromPath(pathParent);
    getInode(inodeNumParent, &inodePathParent);
    
    
    // On vérifie que le fichier pointé par pPathExistant existe et que l'inode pointé par le parent de pPathNouveauLien
    if(inodeNumPathExistant == -1 || inodeNumParent == -1){
        return -1;
    }
    
    // On vérifie que le fichier pointé par pPathNouveauLien n'existe pas
    if(inodeNumPathNouveau != -1){
        return -2;
    }
    
    // On vérifie que pPathExistant n'est pas un directory et qu'il est un fichier normal
    if ((inodePathExistant->iNodeStat.st_mode & G_IFDIR) && !(inodePathExistant->iNodeStat.st_mode & G_IFREG)){
        return -3;
    }
    
    // On vérifie que le dossier parent de pPathNouveau lien n'est pas plein
    if(inodePathParent->iNodeStat.st_size >= BLOCK_SIZE){
        return -4;
    }
    
    
    // On trouve le premier endroit disponible pour mettre le hardlink
    int numEntry = NumberofDirEntry(inodePathParent->iNodeStat.st_size);
    //On incrémente la grosseur du dossier parent du nouveau lien et le nombre de liens de pPathExistant
    inodePathParent->iNodeStat.st_size += sizeof(DirEntry);
    inodePathExistant->iNodeStat.st_nlink++;
    
    // On va chercher le bloc associé au parent
    char data[BLOCK_SIZE];
    ReadBlock(inodePathParent->Block[0], data);
    DirEntry* entries = (DirEntry*) data;
    
    
    
    // On met le numéro d'inode de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    entries[numEntry].iNode = inodeNumPathExistant;
    
    // On met le nom du fichier de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    char nomLien[FILENAME_SIZE];
    GetFilenameFromPath(pPathNouveauLien, nomLien);
    strcpy(entries[numEntry].Filename, nomLien);
    
    // On écrit le bloc modifié sur le disque
    WriteBlock(inodePathParent->Block[0], data);
    
    // Écriture des inodes
    addiNodeToiNodeBlock(inodePathExistant);
    addiNodeToiNodeBlock(inodePathParent);
    
    return 0;
}

int bd_unlink(const char *pFilename) {
	int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    getInode(iNodeNumFilename, &iNodeFilename);
    
    // On regarde si le fichier existe
    if(iNodeNumFilename == -1)return -1;
    
    // On regarde si le fichier est un fichier normal
    if(!(iNodeFilename->iNodeStat.st_mode & G_IFREG))return -2;
    
    
    char parentDir[strlen(pFilename)];
    GetDirFromPath(pFilename, parentDir);
    int iNodeNumParent = getInodeFromPath(parentDir);
    
    iNodeEntry* iNodeParent = alloca(sizeof(*iNodeParent));
    getInode(iNodeNumParent, &iNodeParent);
    
    char data[BLOCK_SIZE];
    ReadBlock(iNodeParent->Block[0], data);
    DirEntry* entries = (DirEntry*) data;
    
    int qtDir = NumberofDirEntry(iNodeParent->iNodeStat.st_size);
    
    char filename[FILENAME_SIZE];
    GetFilenameFromPath(pFilename, filename);

    for(int i = 0; i < qtDir; i++){
        if(strcmp(entries[i].Filename, filename) == 0){
            for (int j = i; j < qtDir - 1; j++) {
                entries[j] = entries[j+1];
                }
            break;
            }
    }
    
    WriteBlock(iNodeParent->Block[0], data);
    
    iNodeParent->iNodeStat.st_size -= sizeof(DirEntry);
    addiNodeToiNodeBlock(iNodeParent);
    
    // On décrémente le nombre de liens
    iNodeFilename->iNodeStat.st_nlink--;
    
    
    // Si on a 0 liens, on doit relacher le iNode
    if(iNodeFilename->iNodeStat.st_nlink == 0){
        if(iNodeFilename->iNodeStat.st_blocks > 0){
            releaseBlock(iNodeFilename->Block[0]);
        }
        releaseInode(iNodeNumFilename);
    } else {
        addiNodeToiNodeBlock(iNodeFilename);
    }
    return 0;
}

int bd_truncate(const char *pFilename, int NewSize) {
    int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    getInode(iNodeNumFilename, &iNodeFilename);
    
    // On regarde si le fichier existe
    if(iNodeNumFilename == -1)return -1;
    
    // On regarde si le fichier est un fichier répertoire
    if(iNodeFilename->iNodeStat.st_mode & G_IFDIR)return -2;
    
    // On vérifie si la nouvelle taille est 0 et qu'on doit simplement relacher le bloc
    if(NewSize == 0){
        releaseBlock(iNodeFilename->Block[0]);
        iNodeFilename->iNodeStat.st_blocks = 0;
        iNodeFilename->iNodeStat.st_size = 0;
        addiNodeToiNodeBlock(iNodeFilename);
        return 0;
    }
    
    // On ajuste tout de suite le size de l'iNode pour savoir combien de '\0' on va devoir mettre
    if(NewSize < iNodeFilename->iNodeStat.st_size){
        iNodeFilename->iNodeStat.st_size = NewSize;
    } else if (NewSize > iNodeFilename->iNodeStat.st_size){
        if (NewSize > MAX_FILE_SIZE) {
            NewSize = MAX_FILE_SIZE;
        }
        char data[BLOCK_SIZE];
        ReadBlock(iNodeFilename->Block[0], data);
        
        // On ajoute les '\0' dans le fichier
        for (int i = iNodeFilename->iNodeStat.st_size; i < NewSize; i++){
            data[i] = '\0';
        }
        // On écrit le bloc
        WriteBlock(iNodeFilename->Block[0], data);
    }
    
    // On écrit l'iNode
    addiNodeToiNodeBlock(iNodeFilename);
    return NewSize;
}

int bd_rmdir(const char *pFilename) {
    // On va chercher le inode du pFilename
    int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    getInode(iNodeNumFilename, &iNodeFilename);
    char filename[FILENAME_SIZE];
    GetFilenameFromPath(pFilename, filename);
    
    // On regarde si le fichier existe
    if(iNodeNumFilename == -1)return -1;
    
    // On regarde si le fichier est bien un répertoire
    if(iNodeFilename->iNodeStat.st_mode & G_IFREG || !(iNodeFilename->iNodeStat.st_mode & G_IFDIR)){
        return -2;
    }
    
    // On regarde si la taille du fichier est bien de 2 (donc contient "." et ".."
    if(iNodeFilename->iNodeStat.st_size > 2*sizeof(DirEntry)){
        return -3;
    }
    
    // On va chercher le inode du parent
    char parentPath[strlen(pFilename)];
    GetDirFromPath(pFilename, parentPath);
    int iNodeNumParent = getInodeFromPath(parentPath);
    iNodeEntry* parentInode = alloca(sizeof(*parentInode));
    getInode(iNodeNumParent, &parentInode);
    
    char data[BLOCK_SIZE];
    ReadBlock(parentInode->Block[0], data);
    DirEntry* entries = (DirEntry*) data;
    
    // On décrémente ele nombre de liens du parent
    parentInode->iNodeStat.st_nlink--;
    
    // On trouve la quantité de fichiers dans le dossier parent
    int qtDir = NumberofDirEntry(parentInode->iNodeStat.st_size);
    
    // On compacte les entrées des fichiers
    for(int i = 0; i < qtDir; i++){
        if(strcmp(entries[i].Filename, filename) == 0){
            for (int j = i; j < qtDir; j++) {
                entries[j] = entries[j+1];
            }
            break;
        }
    }
    
    parentInode->iNodeStat.st_size -= sizeof(DirEntry);
    
    // On écrit nos modifications sur le parent
    WriteBlock(parentInode->Block[0], data);
    addiNodeToiNodeBlock(parentInode);
    
    // On relâche le bloc et le inode associé au répertoire qu'on vient de supprimer
    releaseBlock(iNodeFilename->Block[0]);
    releaseInode(iNodeNumFilename);
    
    return 0;
}

int bd_rename(const char *pFilename, const char *pDestFilename) {
    char parentPath[strlen(pFilename)];
    GetDirFromPath(pFilename, parentPath);
    int iNodeNumFilename = getInodeFromPath(pFilename);
    iNodeEntry* iNodeFilename = alloca(sizeof(*iNodeFilename));
    getInode(iNodeNumFilename, &iNodeFilename);
    char filename[FILENAME_SIZE];
    GetFilenameFromPath(pFilename, filename);
    
    char DestFilename[strlen(pDestFilename)];
    GetDirFromPath(pDestFilename, DestFilename);
    int iNodeNumDestParent = getInodeFromPath(DestFilename);
    iNodeEntry* iNodeDestParent = alloca(sizeof(*iNodeDestParent));
    getInode(iNodeNumDestParent, &iNodeDestParent);
    
    char newFilename[strlen(pDestFilename)];
    GetFilenameFromPath(pDestFilename, newFilename);
    
    int iNodeNumParent = getInodeFromPath(parentPath);
    iNodeEntry* parentInode = alloca(sizeof(*parentInode));
    getInode(iNodeNumParent, &parentInode);
    
    if(iNodeNumFilename == -1 || iNodeNumDestParent == -1) return -1;
    
    
    //////// JUSTE CHANGER LE NOM /////////
    if(strcmp(parentPath, DestFilename) == 0){
        // On trouve la quantité de fichiers dans le dossier parent
        int qtDir = NumberofDirEntry(parentInode->iNodeStat.st_size);
        
        char data[BLOCK_SIZE];
        ReadBlock(parentInode->Block[0], data);
        DirEntry* entries = (DirEntry*) data;
        
        // On compacte les entrées des fichiers
        for(int i = 0; i < qtDir; i++){
            if(strcmp(entries[i].Filename, filename) == 0){
                strcpy(entries[i].Filename, newFilename);
                break;
            }
        }
        
        // On écrit nos modifications sur le parent
        WriteBlock(parentInode->Block[0], data);
        return 0;
    }
    //////////////////////////////////////////
    
    // On vérifie que le dossier parent de pPathNouveau lien n'est pas plein
    if(iNodeDestParent->iNodeStat.st_size >= BLOCK_SIZE){
        return -4;
    }
    
    //////////////// DANS LE CAS D'UN DIRECTORY            ////////////////
    //////////////// SUPPRIMER DU FICHIER PARENT DE DÉPART ////////////////
    
    // On décrémente le nombre de liens du parent
    if (iNodeFilename->iNodeStat.st_mode & G_IFDIR) {
        
        parentInode->iNodeStat.st_nlink--;
        // On trouve la quantité de fichiers dans le dossier parent
        int qtDir = NumberofDirEntry(parentInode->iNodeStat.st_size);
        
        char data[BLOCK_SIZE];
        ReadBlock(parentInode->Block[0], data);
        DirEntry* entries = (DirEntry*) data;
        
        // On compacte les entrées des fichiers
        for(int i = 0; i < qtDir; i++){
            if(strcmp(entries[i].Filename, filename) == 0){
                for (int j = i; j < qtDir; j++) {
                    entries[j] = entries[j+1];
                }
                break;
            }
        }
        parentInode->iNodeStat.st_size -= sizeof(DirEntry);
        
        // On écrit nos modifications sur le parent
        WriteBlock(parentInode->Block[0], data);
        addiNodeToiNodeBlock(parentInode);
        
        /////////////////////////////////////////////////////////////////////
        
        //////////////// AJOUTER AU FICHIER PARENT NOUVEAU ///////////////////
        DirEntry *dirData = (DirEntry*) data;
        qtDir = NumberofDirEntry(iNodeDestParent->iNodeStat.st_size);
        iNodeDestParent->iNodeStat.st_size += sizeof(DirEntry);
        ReadBlock(iNodeDestParent->Block[0], data);

        iNodeDestParent->iNodeStat.st_nlink++;

        // On regarde la quantité de fichiers présents dans le iNode du parent
        

        // On ajoute le nom et le iNode
        strcpy(dirData[qtDir].Filename, filename);
        dirData[qtDir].iNode = iNodeFilename->iNodeStat.st_ino;

        // On écrit nos modifications sur le parent
        WriteBlock(iNodeDestParent->Block[0], data);
        addiNodeToiNodeBlock(iNodeDestParent);
        ///////////////////////////////////////////////////////////////////////

        ///////////// ON MODIFIE SON PROPRE PARENT ////////////////////////////
        ReadBlock(iNodeFilename->Block[0], data);
        dirData = (DirEntry*) data;
        dirData[1].iNode = iNodeNumDestParent;
        WriteBlock(iNodeFilename->Block[0], data);
        ///////////////////////////////////////////////////////////////////////
        
        ///////////// ON VOULAIT EN PLUS MODIFIER LE NOM //////////////////////
        if(strcmp(filename, newFilename) != 0){
            // On trouve la quantité de fichiers dans le dossier parent
            int qtDir = NumberofDirEntry(iNodeDestParent->iNodeStat.st_size);

            char data[BLOCK_SIZE];
            ReadBlock(iNodeDestParent->Block[0], data);
            DirEntry* entries = (DirEntry*) data;
            
            // On trouve le nom et on le modifie
            for(int i = 0; i < qtDir; i++){
                if(strcmp(entries[i].Filename, filename) == 0){
                    strcpy(entries[i].Filename, newFilename);
                    break;
                }
            }

            // On écrit nos modifications sur le parent
            WriteBlock(iNodeDestParent->Block[0], data);
        }
        //////////////////////////////////////////////////////////////////////
    }
    //////////////////// DÉPLACER UN FICHIER /////////////////////////////
    if (iNodeFilename->iNodeStat.st_mode & G_IFREG) {
        bd_hardlink(pFilename, pDestFilename);
        bd_unlink(pFilename);
    }
    //////////////////////////////////////////////////////////////////////
    
    return 0;
}

int bd_readdir(const char *pDirLocation, DirEntry **ppListeFichiers) {
    int iNodeNumDirLocation = getInodeFromPath(pDirLocation);
    iNodeEntry* iNodeDirLocation = alloca(sizeof(*iNodeDirLocation));
    getInode(iNodeNumDirLocation, &iNodeDirLocation);
    
    // On regarde si le fichier existe
    if(iNodeNumDirLocation == -1)return -1;
    
    // On regarde si le fichier est un fichier répertoire
    if(!(iNodeDirLocation->iNodeStat.st_mode & G_IFDIR))return -2;
    
    char data[BLOCK_SIZE];
    ReadBlock(iNodeDirLocation->Block[0], data);
    
    DirEntry* dataEntry = (DirEntry*) data;
    
    *ppListeFichiers = (DirEntry* ) malloc(iNodeDirLocation->iNodeStat.st_size);
    
    memcpy(*ppListeFichiers, dataEntry, iNodeDirLocation->iNodeStat.st_size);
    
    return NumberofDirEntry(iNodeDirLocation->iNodeStat.st_size);
}

int bd_symlink(const char *pPathExistant, const char *pPathNouveauLien) {
    int iNodeNumPathNouveauLien = getInodeFromPath(pPathNouveauLien);
    iNodeEntry* iNodePathNouveauLien = alloca(sizeof(*iNodePathNouveauLien));
    getInode(iNodeNumPathNouveauLien, &iNodePathNouveauLien);
    
    char parentPath[strlen(pPathNouveauLien)];
    GetDirFromPath(pPathNouveauLien, parentPath);
    
    int iNodeNumParentPathNouveauLien = getInodeFromPath(parentPath);
    
    // On regarde si le fichier parent existe
    if(iNodeNumParentPathNouveauLien == -1)return -1;
    
    // On regarde si le fichier existe déjà
    if(iNodeNumPathNouveauLien != -1)return -2;
    
    bd_create(pPathNouveauLien);

    getInode(getInodeFromPath(pPathNouveauLien), &iNodePathNouveauLien);
    iNodePathNouveauLien->iNodeStat.st_mode |= G_IFLNK;
    addiNodeToiNodeBlock(iNodePathNouveauLien);
    bd_write(pPathNouveauLien, pPathExistant, 0, (int)strlen(pPathExistant));
    
    return 0;
}

int bd_readlink(const char *pPathLien, char *pBuffer, int sizeBuffer) {
    int iNodeNumPathLien = getInodeFromPath(pPathLien);
    iNodeEntry* iNodePathLien = alloca(sizeof(*iNodePathLien));
    getInode(iNodeNumPathLien, &iNodePathLien);
    
    if(iNodeNumPathLien == -1)return -1;
    if(!(iNodePathLien->iNodeStat.st_mode & G_IFREG && iNodePathLien->iNodeStat.st_mode & G_IFLNK))return -2;
    
    bd_read(pPathLien, pBuffer, 0, sizeBuffer);
    
    pBuffer[sizeBuffer] = '\0';
    
    return (int)strlen(pBuffer);
}

