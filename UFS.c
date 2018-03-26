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
// Fonction pour trouver le numéro d'inode d'un fichier particulier
int getFileInodeNumber(const char *filename, int parentInodeNumber){
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

int getInodeFromPath(const char *path){
    
    int lastInode = ROOT_INODE;
    char* files;
    char pathCopy[100];
    strcpy(pathCopy, path);
    files = strtok (pathCopy,"/");
    
    while (files != NULL)
    {
        lastInode = getFileInodeNumber(files, lastInode);
        files = strtok (NULL, "/");
        
    }
    
    return lastInode;
     
}


int bd_countfreeblocks(void) {
    int nbrFreeBlock = 0;
    char data[BLOCK_SIZE];
    int errReadBlock;
    
    // On assigne une valeur de retour à la fonction ReadBLock
    errReadBlock = ReadBlock(2, data);

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
    int inode = getInodeFromPath(pFilename);
    
    return 0;
    
}

int bd_create(const char *pFilename) {
	return -1;
}

int bd_read(const char *pFilename, char *buffer, int offset, int numbytes) {
	return -1;
}

int bd_mkdir(const char *pDirName) {
	return -1;
}

int bd_write(const char *pFilename, const char *buffer, int offset, int numbytes) { 
	return -1;
}

int bd_hardlink(const char *pPathExistant, const char *pPathNouveauLien) {
	return -1;
}

int bd_unlink(const char *pFilename) {
	return -1;
}

int bd_truncate(const char *pFilename, int NewSize) {
	return -1;
}

int bd_rmdir(const char *pFilename) {
	return -1;
}

int bd_rename(const char *pFilename, const char *pDestFilename) {
	return -1;
}

int bd_readdir(const char *pDirLocation, DirEntry **ppListeFichiers) {
	return -1;
}

int bd_symlink(const char *pPathExistant, const char *pPathNouveauLien) {
    return -1;
}

int bd_readlink(const char *pPathLien, char *pBuffer, int sizeBuffer) {
    return -1;
}

