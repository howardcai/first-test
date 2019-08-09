Summary: F5 Descriptor Socket Client Library
Name: libdescsock-client
Version: %{version}
Release: dev
Group: Velocity
License: Commercial
Packager: F5 Networks <root@localhost>

%global debug_package %{nil}

%description
Library to encapsulate DMA Agent Client Descriptor Socket interface.

%prep

%post
ldconfig

%preun

%build
make VERSION=%{version} PREFIX=/usr %{?_smp_mflags} library

%install
make VERSION=%{version} PREFIX=/usr DESTDIR=%{?buildroot} install

%clean

%files
%defattr(-,root,root)
/usr/lib/libdescsock-client.so
/usr/lib/libdescsock-client.a
/usr/include/descsock_client.h