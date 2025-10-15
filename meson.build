project('command-not-found', 'c')

# Добавляем зависимости
i18n = import('i18n')
add_project_arguments('-DGETTEXT_PACKAGE="command-not-found"', language: 'c')

# Перевод
subdir('po')

executable('command-not-found', 'src/command-not-found.c',
  install: true,
  install_dir: get_option('bindir'))

# Bash поддержка
install_data('src/shell/bash/command-not-found.sh',
  install_dir: '/etc/bashrc.d')

# Fish поддержка  
install_data('src/shell/fish/command-not-found.fish',
  install_dir: '/etc/fish/conf.d')