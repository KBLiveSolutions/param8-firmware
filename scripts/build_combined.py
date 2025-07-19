#!/usr/bin/env python3
import os
import subprocess
import struct

def build_combined_uf2():
    # Compiler le firmware
    os.system("pio run")
    
    # Créer le filesystem
    os.system("pio run --target buildfs")
    
    # Lire les fichiers
    with open('.pio/build/pico/firmware.bin', 'rb') as f:
        firmware = f.read()
    
    with open('.pio/build/pico/littlefs.bin', 'rb') as f:
        filesystem = f.read()
    
    # Fusionner (filesystem à 2MB)
    firmware_size = len(firmware)
    fs_offset = 0x200000  # 2MB
    
    combined = firmware + b'\xff' * (fs_offset - firmware_size) + filesystem
    
    # Écrire l'image combinée
    with open('combined.bin', 'wb') as f:
        f.write(combined)
    
    # Convertir en UF2
    subprocess.run([
        'python3', 'uf2conv.py', 
        'combined.bin', 
        '--base', '0x10000000',
        '--family', '0xe48bff56',
        '--output', 'combined.uf2'
    ])
    
    print("✅ Fichier combined.uf2 créé avec succès!")

if __name__ == "__main__":
    build_combined_uf2()

# # Rendre le script exécutable
# chmod +x scripts/build_combined.py

# # Exécuter
# python3 scripts/build_combined.py