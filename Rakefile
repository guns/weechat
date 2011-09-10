# -*- encoding: utf-8 -*-

# Copyright (c) 2011 Sung Pae <self@sungpae.com>
# Distributed under the MIT license.
# http://www.opensource.org/licenses/mit-license.php

task :default => :build
task :build   => [:cmake, :make]

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
      ENV['LDFLAGS'] += ' -liconv '
    end
  end
end

desc 'Invoke cmake'
task :cmake => :env do
  mkdir_p 'build'
  Dir.chdir 'build' do
    sh *%W[cmake .. -DPREFIX=#{ENV['PREFIX'] || '/opt/weechat'} -DCMAKE_BUILD_TYPE=None -Wno-dev]
  end
end

desc 'Build weechat'
task :make => :env do
  Dir.chdir 'build' do
    sh *%W[make --jobs=#{ENV['JOBS'] || 2}]
  end
end

desc 'Install weechat'
task :install => :env do
  Dir.chdir 'build' do
    sh *%W[make install]
  end
end
