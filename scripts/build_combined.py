#!/usr/bin/env python3
import os
import subprocess
import struct
import urllib.request

def download_uf2_tools():
    """Télécharge uf2conv.py et uf2families.json si ils n'existent pas"""
    # Aller dans le répertoire racine du projet
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(project_root)
    
    files_to_download = [
        ('uf2conv.py', 'https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2conv.py'),
        ('uf2families.json', 'https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json')
    ]
    
    for filename, url in files_to_download:
        if not os.path.exists(filename):
            print(f"📥 Téléchargement de {filename}...")
            urllib.request.urlretrieve(url, filename)
            if filename.endswith('.py'):
                os.chmod(filename, 0o755)
            print(f"✅ {filename} téléchargé!")

def validate_json_files():
    """Valide les fichiers JSON dans le dossier data"""
    import json
    data_dir = 'data'
    if not os.path.exists(data_dir):
        return
    
    for filename in os.listdir(data_dir):
        if filename.endswith('.json'):
            filepath = os.path.join(data_dir, filename)
            try:
                with open(filepath, 'r') as f:
                    json.load(f)
                print(f"✅ {filename} est un JSON valide")
            except json.JSONDecodeError as e:
                print(f"❌ {filename} contient des erreurs JSON: {e}")
                print(f"   Ligne {e.lineno}, caractère {e.colno}")
                return False
    return True

def debug_littlefs_content():
    """Debug le contenu du filesystem créé"""
    import json
    
    if not os.path.exists('.pio/build/pico/littlefs.bin'):
        print("❌ Pas de littlefs.bin trouvé")
        return
    
    # Vérifier le fichier JSON source
    print("\n🔍 Contenu du fichier JSON source:")
    try:
        with open('data/data.json', 'r') as f:
            data = json.load(f)
        print(f"Mode: {data.get('mode')}")
        
        for preset_key in sorted(data.keys()):
            if preset_key.isdigit():
                preset = data[preset_key]
                encoders = preset.get('encoders', {})
                print(f"Preset {preset_key}: {len(encoders)} encoders")
                
                # Afficher les premiers encoders pour vérification
                for i, (enc_key, enc_value) in enumerate(encoders.items()):
                    if i < 3:  # Afficher seulement les 3 premiers
                        print(f"  Encoder {enc_key}: {enc_value}")
                    elif i == 3:
                        print(f"  ... ({len(encoders) - 3} autres)")
                        break
    except Exception as e:
        print(f"❌ Erreur lors de la lecture du JSON: {e}")

def build_combined_uf2():
    # S'assurer qu'on est dans le répertoire racine du projet
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(project_root)
    print(f"📂 Répertoire de travail: {os.getcwd()}")
    
    # Vérifier la validité des fichiers JSON
    if not validate_json_files():
        print("❌ Arrêt à cause d'erreurs JSON")
        return
    
    # Télécharger les outils UF2 si nécessaire
    download_uf2_tools()
    
    # Vérifier la présence du dossier data
    if os.path.exists('data'):
        data_files = os.listdir('data')
        print(f"📁 Dossier data trouvé avec {len(data_files)} fichier(s): {data_files}")
    else:
        print("⚠️  Pas de dossier data trouvé - création d'un dossier vide...")
        os.makedirs('data', exist_ok=True)
    
    print("🔧 Compilation du firmware...")
    result = os.system(".venv/bin/pio run")
    if result != 0:
        print("❌ Erreur lors de la compilation du firmware")
        return
    
    print("📁 Création du filesystem...")
    result = os.system(".venv/bin/pio run --target buildfs")
    if result != 0:
        print("⚠️  Erreur lors de la création du filesystem, continuons sans...")
    
    debug_littlefs_content()
    
    print("📦 Lecture des fichiers...")
    
    # Vérifier la présence des fichiers
    firmware_path = '.pio/build/pico/firmware.bin'
    littlefs_path = '.pio/build/pico/littlefs.bin'
    
    if not os.path.exists(firmware_path):
        print(f"❌ Fichier firmware introuvable: {firmware_path}")
        return
    
    # Lire le firmware
    with open('.pio/build/pico/firmware.bin', 'rb') as f:
        firmware = f.read()
    
    # Lire le filesystem
    filesystem = b''
    if os.path.exists('.pio/build/pico/littlefs.bin'):
        with open('.pio/build/pico/littlefs.bin', 'rb') as f:
            filesystem = f.read()
        print(f"📁 Filesystem trouvé: {len(filesystem)} bytes")
    
    # CORRECTION : Utiliser la vraie configuration du filesystem
    firmware_size = len(firmware)
    
    # Pour un Pico avec 2MB de flash et 0.5MB de filesystem :
    # Firmware : 0x10000000 à 0x10180000 (1.5MB)
    # Filesystem : 0x10180000 à 0x10200000 (0.5MB)
    fs_offset = 0x180000  # 1.5MB au lieu de 2MB
    
    if filesystem:
        # Vérifier que le firmware ne dépasse pas la zone allouée
        if firmware_size > fs_offset:
            print(f"❌ ERREUR: Firmware trop gros ({firmware_size}) pour la zone allouée ({fs_offset})")
            return
        
        padding_size = fs_offset - firmware_size
        combined = firmware + b'\xff' * padding_size + filesystem
        print(f"📦 Image: firmware({firmware_size}) + padding({padding_size}) + fs({len(filesystem)})")
    else:
        combined = firmware
        print(f"📦 Image: firmware seul ({firmware_size} bytes)")
    
    # Écrire l'image combinée
    with open('combined.bin', 'wb') as f:
        f.write(combined)
    print(f"💾 combined.bin créé: {len(combined)} bytes")
    
    # Convertir en UF2
    print("🔄 Conversion en UF2...")
    result = subprocess.run([
        'python3', './uf2conv.py', 
        'combined.bin', 
        '--base', '0x10000000',
        '--family', '0xe48bff56',
        '--output', 'combined.uf2'
    ], capture_output=True, text=True)
    
    if result.returncode == 0:
        print("✅ Fichier combined.uf2 créé avec succès!")
        print(f"📍 Localisation: {os.path.abspath('combined.uf2')}")
        
        # Afficher la taille du fichier final
        size = os.path.getsize('combined.uf2')
        print(f"📏 Taille: {size} bytes ({size/1024:.1f} KB)")
        
        # Vérifier le contenu du filesystem
        if filesystem:
            print("📋 Contenu du filesystem inclus dans l'image finale")
        else:
            print("⚠️  Aucun filesystem inclus - les données par défaut seront utilisées")
            
    else:
        print("❌ Erreur lors de la conversion UF2:")
        print("STDOUT:", result.stdout)
        print("STDERR:", result.stderr)

def getControlData(controlType, preset, index):
    """Récupère les données de contrôle depuis le JSON"""
    import json
    
    # Convertir les indices en strings pour matcher le format JSON
    presetKey = str(preset)
    indexKey = str(index)
    
    # Charger le fichier JSON
    try:
        with open('data/data.json', 'r') as f:
            doc = json.load(f)
    except Exception as e:
        print(f"❌ Erreur lors de la lecture du JSON: {e}")
        return [0, 0, 0]
    
    # Accéder avec les bonnes clés
    if presetKey in doc and controlType in doc[presetKey] and indexKey in doc[presetKey][controlType]:
        control = doc[presetKey][controlType][indexKey]
        if len(control) >= 3:
            return [control[0], control[1], control[2]]
    
    # Valeurs par défaut si pas trouvé
    return [0, 0, 0]

if __name__ == "__main__":
    build_combined_uf2()