#!/bin/bash

command -v grub-mkrescue >/dev/null 2>&1 ||
{ 
    echo "Ошибка: grub-mkrescue не установлен. Установите пакет grub-pc-bin"
    exit 1
}

ISO_DIR="iso"
OUTPUT_ISO="lumin.iso"
GRUB_CFG="grub.cfg"

TEMP_DIR=$(mktemp -d)
mkdir -p "${TEMP_DIR}/boot/grub"

if [ -d "${ISO_DIR}" ]; then
    echo "Копирую содержимое из ${ISO_DIR}/ в корень ISO..."
    
    if [ "$(ls -A ${ISO_DIR} 2>/dev/null)" ]; then
        cp -r "${ISO_DIR}"/* "${TEMP_DIR}/"
        
        if [ -d "${TEMP_DIR}/boot/grub" ]; then
            echo "Обнаружена существующая папка boot/grub/, конфиг будет перезаписан"
        fi
    else
        echo "Папка ${ISO_DIR} пуста"
    fi
else
    echo "Предупреждение: папка ${ISO_DIR} не существует, создаю пустую"
    mkdir -p "${ISO_DIR}"
fi

cat > "${TEMP_DIR}/boot/grub/${GRUB_CFG}" << 'EOF'
insmod multiboot2
insmod part_msdos
insmod ext2

set timeout=10
set default=0

menuentry "Lumin" {
    multiboot2 /kernel.bin
    
    set gfxpayload=keep
    boot
}

menuentry "Shutdown" {
    halt
}
EOF

echo ""
echo "Создание ISO файла..."
grub-mkrescue -o "${OUTPUT_ISO}" "${TEMP_DIR}"

if [ $? -eq 0 ]; then
    echo ""
    echo "ISO успешно создан: ${OUTPUT_ISO}"
    echo "Размер файла: $(du -h ${OUTPUT_ISO} | cut -f1)"
else
    echo "Ошибка при создании ISO"
fi

rm -rf "${TEMP_DIR}"