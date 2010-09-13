Name:		ivykis
Summary:	event handling library
Group:		System Environment/Libraries
Version:	0.17
Release:	1
URL:		http://libivykis.sourceforge.net/
Source0:	ivykis-0.17.tar.gz
Packager:	Lennert Buytenhek <ivykis@wantstofly.org>
BuildRoot:	/tmp/%{name}-%{version}
License:	LGPLv2.1

%description
ivykis is an event handling library.

%prep
%setup -q -n %{name}-%{version}

%build
make CFLAGS="-O3"

%install
rm -rf %{buildroot}

install -d -m 0755 %{buildroot}%{_bindir}
install -m 0755 misc/ivykis-config %{buildroot}%{_bindir}
install -d -m 0755 %{buildroot}%{_libdir}/pkgconfig
install -m 0644 misc/libivykis.pc %{buildroot}%{_libdir}/pkgconfig

install -d -m 0755 %{buildroot}%{_includedir}
install -m 0644 lib/include/*.h %{buildroot}%{_includedir}
install -m 0644 modules/include/*.h %{buildroot}%{_includedir}

install -d -m 0755 %{buildroot}%{_libdir}
install -m 0755 lib/libivykis.a %{buildroot}%{_libdir}
install -m 0755 modules/libivykis-modules.a %{buildroot}%{_libdir}

install -d -m 0755 %{buildroot}%{_mandir}/man3
install -m 0644 lib/man3/iv*.3 %{buildroot}%{_mandir}/man3
install -m 0644 modules/man3/iv*.3 %{buildroot}%{_mandir}/man3


%files
%defattr(-,root,root)
%{_bindir}/*
%{_includedir}/iv*
%{_libdir}/lib*
%{_libdir}/pkgconfig/*
%{_mandir}/man3/*

%clean
rm -rf %{buildroot}

%changelog
* Sat Sep 13 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.17.

* Sat Sep 10 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.16.

* Sat Sep  4 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.15.

* Mon Aug 16 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.14.

* Wed Aug 11 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Don't forget to install iv_avl.h as well.

* Wed Aug 11 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.13.

* Wed Jun  2 2010 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.12.

* Sun Feb  8 2009 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.11.

* Tue Jan  6 2009 Lennert Buytenhek <buytenh@wantstofly.org>
- Don't forget to install iv_fd_compat.h as well.

* Mon Jan  5 2009 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.10.

* Fri Jan  2 2009 Lennert Buytenhek <buytenh@wantstofly.org>
- Release ivykis 0.9.
