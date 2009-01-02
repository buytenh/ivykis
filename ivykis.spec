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

%description
ivykis is an event handling library.

%prep
%setup -q -n %{name}-%{version}

%build
cd src
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

install -d -m 0755 %{buildroot}%{_mandir}/man3
install -m 0644 doc/iv*.3 %{buildroot}%{_mandir}/man3


%files
%defattr(-,root,root)
%{_bindir}/*
%{_includedir}/iv*
%{_libdir}/libivykis.a
%{_libdir}/pkgconfig/*
%{_mandir}/man3/*

%clean
rm -rf %{buildroot}

%changelog
