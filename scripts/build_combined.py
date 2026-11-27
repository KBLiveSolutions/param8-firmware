#!/usr/bin/env python3
import os
import re
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

    envs = ['param8_simple', 'param8_advanced']
    for env in envs:
        print(f"\n{'='*60}")
        print(f"🔧 Build env: {env}")
        print('='*60)
        result = os.system(f".venv/bin/pio run -e {env}")
        if result != 0:
            print(f"❌ Erreur compilation {env}")
            return
        result = os.system(f".venv/bin/pio run -e {env} --target buildfs")
        if result != 0:
            print(f"⚠️  Erreur filesystem {env}, continuons sans...")
        build_env_uf2(env)

def build_env_uf2(env):
    debug_littlefs_content()

    print("📦 Lecture des fichiers...")

    firmware_path = f'.pio/build/{env}/firmware.bin'
    littlefs_path = f'.pio/build/{env}/littlefs.bin'

    if not os.path.exists(firmware_path):
        print(f"❌ Fichier firmware introuvable: {firmware_path}")
        return

    with open(firmware_path, 'rb') as f:
        firmware = f.read()

    filesystem = b''
    if os.path.exists(littlefs_path):
        with open(littlefs_path, 'rb') as f:
            filesystem = f.read()
        print(f"📁 Filesystem trouvé: {len(filesystem)} bytes")
    
    # CORRECTION : Utiliser la vraie configuration du filesystem
    firmware_size = len(firmware)
    fs_offset = 0x180000  # Filesystem starts at 1.5MB

    if firmware_size > fs_offset:
        print(f"❌ ERREUR: Firmware trop gros ({firmware_size}) pour la zone allouée ({fs_offset})")
        return

    # Lire la version depuis version.h
    version = "1.0.0"
    try:
        with open('src/core/version.h', 'r') as f:
            content = f.read()
        major = re.search(r'FW_VERSION_MAJOR\s+(\d+)', content).group(1)
        minor = re.search(r'FW_VERSION_MINOR\s+(\d+)', content).group(1)
        patch = re.search(r'FW_VERSION_PATCH\s+(\d+)', content).group(1)
        version = f"{major}.{minor}.{patch}"
    except Exception:
        pass

    suffix = "" if env == "param8_simple" else "-advanced"

    # Firmware-only UF2 (for OTA updates — does not touch user settings)
    fw_bin = f"firmware{suffix}.bin"
    fw_uf2 = f"releases/param8-v{version}-firmware{suffix}.uf2"
    with open(fw_bin, 'wb') as f:
        f.write(firmware)
    print(f"💾 {fw_bin} créé: {firmware_size} bytes (firmware only)")

    def to_uf2(src_bin, dst_uf2):
        r = subprocess.run([
            'python3', './uf2conv.py', src_bin,
            '--base', '0x10000000',
            '--family', '0xe48bff56',
            '--output', dst_uf2
        ], capture_output=True, text=True)
        if r.returncode == 0:
            print(f"✅ {dst_uf2} ({os.path.getsize(dst_uf2)/1024:.1f} KB)")
        else:
            print(f"❌ Erreur UF2 {dst_uf2}:", r.stderr)
        return r.returncode == 0

    print("🔄 Conversion en UF2...")
    to_uf2(fw_bin, fw_uf2)

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