Name:		ivykis
Summary:	event handling library
Group:		System Environment/Libraries
Version:	0.8
Release:	1
URL:		http://libivykis.sourceforge.net/
Source0:	ivykis-0.8.tar.gz
Packager:	Lennert Buytenhek <ivykis@wantstofly.org>
BuildRoot:	/tmp/%{name}-%{version}
License:	LGPL

%package -n ivykis-debug
Summary:	debug version of ivykis
Group:		System Environment/Libraries
Requires:	ivykis = 0.8

%description
ivykis is an event handling library.

%description -n ivykis-debug
ivykis-debug is a debug version of the ivykis event handling library.

%prep
%setup -q -n %{name}-%{version}

%build
cd src
make CFLAGS="-DIV_DEBUG=1 -g"
cp libivykis.a libivykis-debug.a
make clean
make CFLAGS="-O3"

%install
rm -rf %{buildroot}

install -d -m 0755 %{buildroot}%{_bindir}
install -m 0755 misc/ivykis-config %{buildroot}%{_bindir}
install -d -m 0755 %{buildroot}%{_libdir}/pkgconfig
install -m 0644 misc/libivykis.pc %{buildroot}%{_libdir}/pkgconfig

install -d -m 0755 %{buildroot}%{_includedir}
install -m 0644 src/iv.h %{buildroot}%{_includedir}
install -m 0644 src/iv_list.h %{buildroot}%{_includedir}

install -d -m 0755 %{buildroot}%{_libdir}
install -m 0755 src/libivykis.a %{buildroot}%{_libdir}
install -m 0755 src/libivykis-debug.a %{buildroot}%{_libdir}

install -d -m 0755 %{buildroot}%{_mandir}/man3
install -m 0644 doc/iv*.3 %{buildroot}%{_mandir}/man3


%files
%defattr(-,root,root)
%{_bindir}/*
%{_includedir}/iv*
%{_libdir}/libivykis.a
%{_libdir}/pkgconfig/*
%{_mandir}/man3/*

%files -n ivykis-debug
%defattr(-,root,root)
%{_libdir}/libivykis-debug.a

%clean
rm -rf %{buildroot}

%changelog
