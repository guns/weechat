# -*- encoding: utf-8 -*-

# Copyright (c) 2011 Sung Pae <self@sungpae.com>
# Distributed under the MIT license.
# http://www.opensource.org/licenses/mit-license.php

task :default => :build

task :env do
  ENV['CFLAGS' ] ||= ''
  ENV['LDFLAGS'] ||= ''

  if RUBY_PLATFORM =~ /darwin/i
    # Homebrew linking help
    if system 'command -v brew &>/dev/null'
      %w[libgcrypt libgpg-error gettext gnutls].each do |pkg|
        cellar = %x(brew --prefix #{pkg}).chomp
        ENV['CFLAGS' ] += " -I#{cellar}/include "
        ENV['LDFLAGS'] += " -L#{cellar}/lib "
      end
      ENV['CFLAGS' ] += ' -DHAVE_GNUTLS '
      ENV['LDFLAGS'] += ' -liconv -lgnutls '
    end
  end
end

desc 'Invoke cmake'
task :cmake => :env do
  cmd = %W[cmake .. -DPREFIX=#{ENV['PREFIX'] || '/opt/weechat'} -DCMAKE_BUILD_TYPE=None -Wno-dev]

  # Arch Linux has boldly moved to Python 3, but nobody's following
  cmd << '-DPYTHON_EXECUTABLE=/usr/bin/python2' if %x(uname -a) =~ /ARCH/

  mkdir_p 'build'
  Dir.chdir('build') { sh *cmd }
end

desc 'Invoke make'
task :make => :env do
  Dir.chdir 'build' do
    sh *%W[make --jobs=#{ENV['JOBS'] || 2}]
  end
end

desc 'Build weechat'
task :build => [:cmake, :make]

desc 'Install weechat'
task :install => :env do
  Dir.chdir 'build' do
    sh *%W[make install]
  end
end
