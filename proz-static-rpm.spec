%define name 	proz
%define version	2.0.5beta
#%define buildversion static
%define release	1
%define prefix	/usr

Summary: 	An GUI advanced Linux download manager
Summary(fr): 	Un gestionnaire graphique de téléchargement avancé pour Linux
Name: 		%{name}
Version: 	%{version}
Release: 	%{release}
Copyright: 	GNU
BuildArchitectures:	i386
Group: 		Networking/File transfer
Source0: 	%{name}-%{version}.tar.gz
Url: 		http://prozilla.genesys.ro/
BuildRoot: 	/var/tmp/%{name}-%{version}-root

%description
This is the GUI version of Prozilla. It uses libprozilla and the GUI is
created and designed with The Fast Light Tool Kit (fltk).

ProZilla is a download accellerator program written for Linux to speed up the
normal file download process. It often gives speed increases of around 200% to
300%. It supports both FTP and HTTP protocols, and the theory behind it is
very simple. The program opens multiple connections to several ftp servers
hosting the same file, and each of the connections downloads a part of the file,
thus defeating existing internet congestion prevention methods which slow down
a single connection based download. 

It also features the new multiple server based downloading, based on
ftpsearch and ping returned results prozilla now supports downloading from
multiple servers simultaneously.

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
rm -rf $RPM_BUILD_ROOT/%{name}-%{version}

%setup -n %{name}-%{version}
CFLAGS="$RPM_OPT_FLAGS -static" CXXFLAGS="$RPM_OPT_FLAGS -static" ./configure --prefix=%{prefix} --sysconfdir=/etc



%build
make

%install
make prefix=$RPM_BUILD_ROOT%{prefix} install


# Mandrake Menu entry
mkdir -p $RPM_BUILD_ROOT%{_menudir}
cat <<EOF > $RPM_BUILD_ROOT%{_menudir}/%{name}
?package(%{name}): \
needs="x11" \
section="Networking/File transfer" \
title="ProzGUI" \
longtitle="ProzGUI" \
command="%{_bindir}/proz" \
icon="%{name}.xpm"
EOF

mkdir -p $RPM_BUILD_ROOT%{_miconsdir} $RPM_BUILD_ROOT%{_liconsdir} $RPM_BUILD_ROOT%{_iconsdir}


%post
%{update_menus}

%postun
%{clean_menus}


%clean
rm -rf $RPM_BUILD_ROOT
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%files
%{prefix}/bin/proz
%{prefix}/include/proz*
%{prefix}/lib/libprozilla*
%{prefix}/share/locale/*/LC_MESSAGES/*
%{prefix}/man/man1/proz*
%doc COPYING
%doc ChangeLog
%doc CREDITS*
%doc INSTALL
%doc README
%doc TODO
%doc docs/FAQ
%doc libprozilla/docs/HACKING


%changelog
* Mon Dec  3 2001 Eric Lassauge <lassauge@mail.dotcom.fr>
- Added french translation

* Thu Sep 27 2001 Grendel <kalum@delrom.ro> 
- This spec file created for making statically linked RPM's

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

* Wed Aug 01 2001 Ralph Slooten <axllent@axllent.cjb.net>
- RPM created for libprozilla / proz
- Created Mandrake Menu-entries

