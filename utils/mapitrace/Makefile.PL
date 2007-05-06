use ExtUtils::MakeMaker;
WriteMakefile(
	      'NAME' => 'MAPI::Mapitrace',
	      'VERSION_FROM' => 'lib/MAPI/EcDoRpc.pm',
	      'EXE_FILES' => [ 'mapitrace' ],
	      );
