#include "fsutil2.h"
#include "bitmap.h"
#include "cache.h"
#include "debug.h"
#include "directory.h"
#include "file.h"
#include "filesys.h"
#include "free-map.h"
#include "fsutil.h"
#include "inode.h"
#include "off_t.h"
#include "partition.h"
#include "../interpreter.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>


#define SECTOR_SIZE 512
#define NUM_SECTORS 2048
#define RECOVERED_DIR "./recovered_files/"

// structure to hold file data temporarily
struct file_data {
    char *content;
    size_t size;
};

extern struct bitmap *free_map;



int copy_in(char *fname) {
    // Open the source file for reading
    FILE *source_file = fopen(fname, "rb");
    if (source_file == NULL) {
        printf("Error: Unable to open file %s for reading\n", fname);
        return handle_error(FILE_DOES_NOT_EXIST);
    }

    // Get the size of the source file
    fseek(source_file, 0, SEEK_END);
    long file_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);

    // Allocate memory to store the file content
    char *file_content = (char *)malloc(file_size+1);
    if (file_content == NULL) {
        fclose(source_file);
        printf("Error: Memory allocation failed\n");
        return -1;
    }

    // Read the content of the source file
    size_t bytes_read = fread(file_content, 1, file_size, source_file);
    fclose(source_file);

    // Check if the file was read successfully
    if (bytes_read != file_size) {
        free(file_content);
        printf("Error: Failed to read file %s\n", fname);
        return -1;
    }

    // Create the file in the shell's hard drive
    if (fsutil_create(fname, bytes_read+1) != 1) {
        free(file_content);
        printf("Error: Failed to create file %s in the shell's hard drive\n", fname);
        return -1;
    }
		
		file_content[bytes_read] = '\0';
    // Write the content to the created file
    if (fsutil_write(fname, file_content, bytes_read+1) == 0) {
        free(file_content);
        printf("Error: Failed to write content to file %s in the shell's hard drive\n", fname);
        return -1;
    }

    free(file_content);
    return 0;
}


int copy_out(char *fname) {
    // Open the file from the shell's hard drive for reading
    int fd = fsutil_seek(fname, 0);
    if (fd == -1) {
        printf("Error: Unable to open file %s from the shell's hard drive\n", fname);
        return -1;
    }

    // Get the size of the file
    int file_size = fsutil_size(fname);
    if (file_size == -1) {
        printf("Error: Unable to get size of file %s in the shell's hard drive\n", fname);
        fsutil_close(fname);
        return -1;
    }

    // Allocate memory to store the file content
    char *file_content = (char *)malloc(file_size);
    if (file_content == NULL) {
        printf("Error: Memory allocation failed\n");
        fsutil_close(fname);
        return -1;
    }

		
		int read_len;
    // Read the content of the file from the shell's hard drive
    if ((read_len=fsutil_read(fname, file_content, file_size)) < 0) {
        free(file_content);
        printf("Error: Failed to read content of file %s from the shell's hard drive\n", fname);
        fsutil_close(fname);
        return -1;
    }
    
    
    // Create the file in the current directory of the real hard drive
    FILE *dest_file = fopen(fname, "wb");
    if (dest_file == NULL) {
        free(file_content);
        printf("Error: Unable to create file %s in the current directory\n", fname);
        fsutil_close(fname);
        return -1;
    }

    // Write the content to the created file
    fwrite(file_content, 1, strlen(file_content), dest_file);
    fclose(dest_file);
    free(file_content);

    fsutil_close(fname);
    return 0;
}


void find_file(char *pattern) {
    // Open the root directory
    struct dir *root_dir = dir_open_root();
    if (root_dir == NULL) {
        printf("Error: Failed to open root directory\n");
        return;
    }

    // Iterate through all files in the root directory
    struct dir_entry e;
    while (dir_readdir(root_dir, e.name)) {
        // Open each file
        struct inode *inode;
        if (!dir_lookup(root_dir, e.name, &inode))
            continue;

        // Check if the file is a regular file
        if (!inode_is_directory(inode)) {
            // Open the file
            struct file *file = file_open(inode);
            if (file != NULL) {
                // Read the content of the file
                char file_content[NUM_SECTORS*SECTOR_SIZE];
                memset(file_content, 0, sizeof(file_content));
                file_read_at(file, file_content, inode_length(inode), 0);

                // Check if the pattern exists in the file content
                if (strstr(file_content, pattern) != NULL) {
                    printf("%s\n", e.name);
                }

                // Close the file
                file_close(file);
            }
        }
    }
    
    
    // Close the root directory
    dir_close(root_dir);
    
}


void fragmentation_degree() {
    int fragmented_files = 0;
    int fragmentable_files = 0;

    // Open the root directory
    struct dir *root_dir = dir_open_root();
    if (root_dir == NULL) {
        printf("Error: Failed to open root directory\n");
        return;
    }

    // Iterate through all files in the root directory
    struct dir_entry e;
    while (dir_readdir(root_dir, e.name)) {
        // Open each file
        struct inode *inode;
        if (!dir_lookup(root_dir, e.name, &inode))
            continue;

        // Check if the file is a regular file and has more than one data block
        if (!inode_is_directory(inode) && inode->data.length > BLOCK_SECTOR_SIZE) {
            fragmentable_files++;

            // Check for fragmentation in the file
            bool fragmented = false;
            
            for (int i = 1; i < DIRECT_BLOCKS_COUNT; i++) {
                if (inode->data.direct_blocks[i] - inode->data.direct_blocks[i-1] > 3 && inode->data.direct_blocks[i] > 0 ) {
                    fragmented = true;
                    break;
                }
                
            }
            
            if (fragmented)
                fragmented_files++;
        }
    }

    // Close the root directory
    dir_close(root_dir);
		
		printf("Num fragmentable files: %d\n",fragmentable_files);
		printf("Num fragmented files: %d\n", fragmented_files);
		
    // Calculate and print the fragmentation degree
    if (fragmentable_files > 0) {
        double fragmentation_degree = (double)fragmented_files / fragmentable_files;
        printf("Fragmentation pct: %.6f\n", fragmentation_degree);
    } else {
        printf("No fragmentable files found\n");
    }
}


// Helper function to read file content into memory
struct file_data read_file_content(struct inode *inode) {
    struct file_data data;
    data.size = inode_length(inode);
    data.content = malloc(data.size);
    if (data.content == NULL) {
        data.size = 0;
        return data;
    }

    struct file *file = file_open(inode);
    if (file == NULL) {
        data.size = 0;
        free(data.content);
        return data;
    }

    file_read_at(file, data.content, data.size, 0);
    file_close(file);

    return data;
}


// Helper function to write file content from memory to disk
bool write_file_content(struct inode *inode, struct file_data data) {
    if (inode_length(inode) != data.size) {
        return false; // Size mismatch, cannot write content
    }

    struct file *file = file_open(inode);
    if (file == NULL) {
        return false; // Unable to open file
    }

    file_write_at(file, data.content, data.size, 0);
    file_close(file);

    return true;
}


int defragment() {
    // Open the root directory
    struct dir *root_dir = dir_open_root();
    if (root_dir == NULL) {
        printf("Error: Failed to open root directory\n");
        return -1;
    }

    // Iterate through all files in the root directory
    struct dir_entry e;
    while (dir_readdir(root_dir, e.name)) {
        // Open each file
        struct inode *inode;
        if (!dir_lookup(root_dir, e.name, &inode))
            continue;

        // Check if the file is a regular file and has more than one data block
        if (!inode_is_directory(inode) && inode->data.length > BLOCK_SECTOR_SIZE) {
            // Check for fragmentation in the file
            bool fragmented = false;
            for (int i = 1; i < DIRECT_BLOCKS_COUNT; i++) {
                if (inode->data.direct_blocks[i] - inode->data.direct_blocks[i-1] > 3 && inode->data.direct_blocks[i] > 0 ) {
                    fragmented = true;
                    break;
                }
            }

            // If fragmented, rearrange the data blocks
            if (fragmented) {
                // Allocate memory to hold the content of the fragmented file
                char *file_content = malloc(inode->data.length);
                if (file_content == NULL) {
                    printf("Error: Memory allocation failed\n");
                    continue;
                }

                // Read the content of the fragmented file into memory
                struct file *file = file_open(inode);
                if (file == NULL) {
                    printf("Error: Failed to open file\n");
                    free(file_content);
                    continue;
                }
                file_read_at(file, file_content, inode->data.length, 0);
                file_close(file);

                // Write the content of the file back to disk, rearranging the blocks
                block_sector_t next_sector = inode->data.direct_blocks[0];
                for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
                    if (i < inode->data.length / BLOCK_SECTOR_SIZE) {
                        block_write(fs_device, next_sector, file_content + i * BLOCK_SECTOR_SIZE);
                        inode->data.direct_blocks[i] = next_sector;
                        next_sector++;
                    } else {
                        break;
                    }
                }

                // Update the file size in the inode
                inode->data.length = inode->data.length;

                // Free memory used to hold file content
                free(file_content);
            }
        }
    }

    // Close the root directory
    dir_close(root_dir);

    printf("Defragmentation completed successfully\n");
    return 0;
}




// Helper function to check if a given sector is free
bool is_sector_free(block_sector_t sector) {
    // Check if the sector is within the valid range
    if (sector >= block_size(fs_device) || sector >= bitmap_size(free_map)) {
        return false; // Sector out of range
    }
    
    
    // Check if the sector is marked as free in the free map
    return bitmap_test(free_map, sector);
}



// Helper function to recover deleted files (flag 0)
void recover_deleted_files() {
    // Open the root directory
    struct dir *root_dir = dir_open_root();
    if (root_dir == NULL) {
        printf("Error: Failed to open root directory\n");
        return;
    }
    
    // Scan all free sectors on the drive
    for (block_sector_t sector = 0; sector < block_size(fs_device); sector++) {
    		    
        if (is_sector_free(sector)) {
            
            // Check if the sector contains a deleted inode
            struct inode *inode = inode_open(sector);
            if (inode != NULL) {
           			
                // Recover the inode
                char filename[50];
                snprintf(filename, sizeof(filename), "recovered0-%d", sector);
                // Create a new file with recovered inode
                FILE *recovered_file = fopen(filename, "w");
                if (recovered_file != NULL) {
                    // Write recovered data to the file
                    fwrite(inode, sizeof(struct inode), 1, recovered_file);
                    fclose(recovered_file);
                    printf("Recovered deleted file: %s\n", filename);
                } else {
                    printf("Error: Failed to create recovered file: %s\n", filename);
                }
                inode_close(inode);
            }
        }
    }
    dir_close(root_dir);
}

// Helper function to check if a sector contains non-zero data
bool is_sector_nonzero(block_sector_t sector) {
    char buffer[SECTOR_SIZE];
    block_read(fs_device, sector, buffer);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        if (buffer[i] != 0) {
            return true; // Non-zero data found
        }
    }
    return false; // Sector is all zero
}




// Helper function to recover hidden data beyond the end of a file
void recover_hidden_data(const char *filename) {
    struct inode *inode = NULL;
    struct dir *dir = dir_open_path("/");
    if (dir != NULL) {
        dir_lookup(dir, filename, &inode);
        dir_close(dir);
    }
    
    if (inode != NULL) {
        off_t file_size = inode_length(inode);
        size_t sector_count = bytes_to_sectors(file_size);

        char recovered_filename[1000];
        FILE *recovered_file;

        for (size_t i = 0; i < sector_count; i++) {
            block_sector_t sector;
            if (i < DIRECT_BLOCKS_COUNT) {
                sector = inode->data.direct_blocks[i];
            } else if (i < DIRECT_BLOCKS_COUNT + INDIRECT_BLOCKS_PER_SECTOR) {
                block_sector_t indirect_block[INDIRECT_BLOCKS_PER_SECTOR];
                block_read(fs_device, inode->data.indirect_block, indirect_block);
                sector = indirect_block[i - DIRECT_BLOCKS_COUNT];
            } else {
                block_sector_t doubly_indirect_block[INDIRECT_BLOCKS_PER_SECTOR];
                block_read(fs_device, inode->data.doubly_indirect_block, doubly_indirect_block);
                block_sector_t indirect_block[INDIRECT_BLOCKS_PER_SECTOR];
                block_read(fs_device, doubly_indirect_block[(i - DIRECT_BLOCKS_COUNT - INDIRECT_BLOCKS_PER_SECTOR) / INDIRECT_BLOCKS_PER_SECTOR], indirect_block);
                sector = indirect_block[(i - DIRECT_BLOCKS_COUNT - INDIRECT_BLOCKS_PER_SECTOR) % INDIRECT_BLOCKS_PER_SECTOR];
            }

            if (i == sector_count - 1) {
                off_t last_block_offset = file_size % SECTOR_SIZE;
                if (last_block_offset < SECTOR_SIZE) {
                    char buffer[SECTOR_SIZE];
                    block_read(fs_device, sector, buffer);
                    if (is_sector_nonzero(sector)) {
                        sprintf(recovered_filename, "recovered2-%s.txt", filename);
                        recovered_file = fopen(recovered_filename, "w");
                        if (recovered_file != NULL) {
                            // Create a new buffer to hold non-null characters
												    char non_null_buffer[SECTOR_SIZE];
												    int j = 0;
												    for (int k = last_block_offset; k < SECTOR_SIZE; k++) {
												    		if (buffer[k] != '\0')
												        	non_null_buffer[j++] = buffer[k];
												    }
												    // Write non-null buffer to the file
												    fwrite(non_null_buffer, sizeof(char), j, recovered_file);
												    fclose(recovered_file);
                        }
                    }
                }
            }
        }
    }
}





void recover(int flag) {
  if (flag == 0) { // recover deleted inodes
  		
			recover_deleted_files();
  } 
  else if (flag == 1) { // recover all non-empty sectors
			FILE *recovered_file;
      char filename[50];
      int sector;

      for (sector = 4; sector < block_size(fs_device); sector++) {
          if (is_sector_nonzero(sector)) {
              sprintf(filename, "recovered1-%d.txt", sector);
              recovered_file = fopen(filename, "w");
              if (recovered_file == NULL) {
                  fprintf(stderr, "Error: Failed to create recovered file for sector %d\n", sector);
                  continue;
              }
              // Write sector contents to file
              char buffer[SECTOR_SIZE];
              block_read(fs_device, sector, buffer);
              fwrite(buffer, sizeof(char), strlen(buffer), recovered_file);
              fclose(recovered_file);
          }
      }
      printf("Recovery completed for flag 1\n");
   
  } 
  else if (flag == 2) { // data past end of file.
			struct dir *dir = dir_open_path("/");
      if (dir != NULL) {
          struct dir_entry entry;
          while (dir_readdir(dir, entry.name)) {
              recover_hidden_data(entry.name);
          }
          dir_close(dir);
      }
      printf("Recovery completed for flag 2\n");
  }
}
