# Maintainer: Sung Pae <self@sungpae.com>
pkgname=weechat-nerv
pkgver=
pkgrel=1
pkgdesc="Custom weechat build"
arch=('x86_64')
url="https://github.com/guns/weechat"
license=('GPL')
groups=('nerv')
depends=('gnutls' 'curl' 'libgcrypt')
makedepends=('git' 'cmake' 'pkg-config' 'perl' 'python2' 'lua' 'tcl' 'ruby' 'aspell' 'guile')
optdepends=('perl' 'python2' 'lua' 'tcl' 'ruby' 'aspell' 'guile')
provides=('weechat')
conflicts=('weechat')
replaces=('weechat-guns')

pkgver() {
    printf %s "$(git describe --long --tags | tr - .)"
}

build() {
    cd "$startdir"
    PREFIX=/usr JOBS=2 rake build
}

package() {
    cd "$startdir"
    rake DESTDIR="$pkgdir/" install
}
