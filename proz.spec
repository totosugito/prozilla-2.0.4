%define name 	proz
%define version	2.0.0
%define release	1
%define prefix	/usr

Summary: 	An GUI advanced Linux download manager
Summary(fr): 	Un gestionnaire graphique de téléchargement avancé pour Linux
Name: 		%{name}
Version: 	%{version}
Release: 	%{release}
License: 	GNU General Public License
Group: 		Applications/Internet
Source0: 	%{name}-%{version}.tar.gz
Url: 		http://prozilla.genesys.ro/
BuildRoot: 	/var/tmp/%{name}-%{version}-root

%description
This is the GUI version of Prozilla. It uses libprozilla and the GUI is
created and designed with The Fast Light Tool Kit (fltk).

ProZilla is a download accellerator program written for Linux to speed up the
normal file download process. It often gives speed increases of around 200% to
300%. It supports both FTP and HTTP protocols, and the theory behind it is
very simple. The program opens multiple connections to a server, and each of
the connections downloads a part of the file, thus defeating existing internet
congestion prevention methods which slow down a single connection based download.

%description -l fr
Ce programme est la version IHM de Prozilla. Il utilise libprozilla et l'IHM
est créé et géré avec les widgets FLTK (The Fast Light Tool Kit).

Prozilla est un accélérateur de téléchargement écris pour Linux pour accélérer
le processus de téléchargement de fichiers. Il permet en général un gain de
vitesse d'environ 200 à 300%. Il supporte les protocoles FTP et HTTP, et la
théorie derrière est très simple: le programme ouvre plusieurs connexions au
serveur, et chacune télécharge une partie du fichier, ce qui permet de combattre
les méthodes de prévention des congestions d'internet qui ralentissent les
téléchargement basés sur une seule connexion.

%prep
rm -rf $RPM_BUILD_ROOT

%setup
%configure

%build
make

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_bindir}
%{_datadir}
%{_mandir}
%doc AUTHORS
%doc COPYING
%doc ChangeLog
%doc CREDITS*
%doc INSTALL
%doc NEWS
%doc README
%doc TODO
%doc docs/FAQ

%package devel

Group: Development/Libraries
License: GNU General Public License
Summary: libprozilla development headers and libraries

%description devel
This is libprozilla, the development headers and libraries used
in prozilla to support the FTP and HTTP protocols.

%files devel
%{_includedir}
%{_libdir}/*.a
%{_libdir}/*.la
%doc libprozilla/AUTHORS
%doc libprozilla/COPYING
%doc libprozilla/INSTALL
%doc libprozilla/NEWS
%doc libprozilla/README
%doc libprozilla/TODO
%doc libprozilla/docs/HACKING

%changelog
* Mon Aug 29 2005 Richard Dawe <rich@phekda.gotadsl.co.uk> 2.0.0-1
- Update for prozilla 2.0.0.
- Remove Mandrake-isms.
- Split development headers and libraries out into -devel.

* Mon Dec  3 2001 Eric Lassauge <lassauge@mail.dotcom.fr>
- Added french translation

* Sun Sep 23 2001 Ralph Slooten <axllent@axllent.cjb.net>
- ProzGUI 2.0.4beta released
- GUI improvements
- FTP search
- Download from several servers

* Wed Aug 29 2001 Ralph Slooten <axllent@axllent.cjb.net>
- ProzGUI 2.0.2 released
- GUI improvements
- Man page included

* Thu Aug 23 2001 Ralph Slooten <axllent@axllent.cjb.net>
- ProzGUI 2.0.0 released!

* Mon Aug 08 2001 Ralph Slooten <axllent@axllent.cjb.net>
- RPM created for libprozilla / proz
- Created Mandrake Menu-entries

