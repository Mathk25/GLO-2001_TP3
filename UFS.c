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
    
    // On sépare la path pour chaque file selon les /
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


int bd_countfreeblocks(void) {
    int nbrFreeBlock = 0;
    char data[BLOCK_SIZE];
    int errReadBlock;
    
    // On assigne une valeur de retour à la fonction ReadBLock
    errReadBlock = ReadBlock(FREE_BLOCK_BITMAP, data);

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
    
    if (inodeNum == -1) return -1;
    
    iNodeEntry* inode = alloca(sizeof(*inode));
    getInode(inodeNum, &inode);
    
    pStat->st_blocks = inode->iNodeStat.st_blocks;
    pStat->st_ino = inode->iNodeStat.st_ino;
    pStat->st_mode = inode->iNodeStat.st_mode;
    pStat->st_nlink = inode->iNodeStat.st_nlink;
    pStat->st_size = inode->iNodeStat.st_size;
    
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
    // On va chercher les numéros d'inodes de pPathExistant et de pPathNouveauLien
    int inodeNumPathExistant = getInodeFromPath(pPathExistant);
    int inodeNumPathNouveau = getInodeFromPath(pPathNouveauLien);
    
    // On va chercher l'inode correspondant à pPathExistant
    iNodeEntry* inodePathExistant  = alloca(sizeof(*inodePathExistant));
    getInode(inodeNumPathExistant, &inodePathExistant);
    
    // On va chercher le path du parent de pPathNouveauLien, ainsi que l'inode associé à celui-ci
    char pathParent[4096];
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
    
   //On incrémente la grosseur du dossier parent du nouveau lien et le nombre de liens de pPathExistant
    inodePathParent->iNodeStat.st_size += sizeof(DirEntry);
    inodePathExistant->iNodeStat.st_nlink++;
    
    // On va chercher le bloc associé au parent
    char data[BLOCK_SIZE];
    ReadBlock(inodePathParent->Block[0], data);
    DirEntry* entries = (DirEntry*) data;
    
    // On trouve le premier endroit disponible pour mettre le hardlink
    int numEntry = NumberofDirEntry(inodePathParent->iNodeStat.st_size);
    
    // On met le numéro d'inode de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    entries[numEntry].iNode = inodeNumPathExistant;
    
    // On met le nom du fichier de pPathExistant dans l'entrée du dossier parent lié au fichier hardlinké
    char nomLien[FILENAME_SIZE];
    GetFilenameFromPath(pPathNouveauLien, nomLien);
    strcpy(entries[numEntry].Filename, nomLien);
    
    // On écrit le bloc modifié sur le disque
    WriteBlock(inodePathParent->Block[0], data);
    
    // MANQUE L'ÉCRITURE DES INODES
    
    return 0;
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

