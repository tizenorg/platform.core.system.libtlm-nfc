Name: libtlm-nfc
Summary: Library for storing user credentials on NFC tag
Version: 0.0.0
Release: 1
Group: Security/Accounts
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
Source1: %{name}.manifest
URL: https://github.com/01org/libtlm-nfc
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires: neard
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-2.0)

%description
%{summary}.


%package devel
Summary: Development files for %{name}
Group: SDK/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
%{summary}.


%package doc
Summary: Documentation files for %{name}
Group: SDK/Documentation
Requires: %{name}-devel = %{version}-%{release}

%description doc
%{summary}


%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1} .


%build
%configure
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install


%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%doc AUTHORS COPYING.LIB INSTALL NEWS README
%{_libdir}/%{name}.so.*


%files devel
%defattr(-,root,root,-)
%{_libdir}/%{name}.so
%{_libdir}/pkgconfig/%{name}.pc


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/%{name}/*

