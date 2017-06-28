// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WallRunCharacter.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine.h" 

#define LEFT -90
#define RIGHT 90

//////////////////////////////////////////////////////////////////////////
// AWallRunCharacter

AWallRunCharacter::AWallRunCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);


	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)


}

//////////////////////////////////////////////////////////////////////////
// Input

void AWallRunCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AWallRunCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AWallRunCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AWallRunCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AWallRunCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AWallRunCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AWallRunCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWallRunCharacter::OnResetVR);
}


void AWallRunCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AWallRunCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	//Jump();
}

void AWallRunCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	//StopJumping();
}

void AWallRunCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AWallRunCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AWallRunCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AWallRunCharacter::MoveRight(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}


void AWallRunCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);





	if (GetCharacterMovement()->IsFalling())
	{
		FHitResult HitResultForward;
		FHitResult HitResultLeft;
		FHitResult HitResulRight;
		FCollisionQueryParams TraceParams = FCollisionQueryParams(FName(TEXT("Trace")), true, this);

		ECollisionChannel Channel = ECC_WorldStatic;

		FVector Start = GetActorLocation();
		FVector End = GetActorRightVector() * PlayerToWallDistance;
		FVector ForwardEnd = GetActorForwardVector() * PlayerToWallDistance;
		
		if (GetWorld()->LineTraceSingleByChannel(HitResultLeft, Start, Start + -End, Channel, TraceParams))
		{
			if (GetWorld()->LineTraceSingleByChannel(HitResultForward, Start, Start + ForwardEnd, Channel, TraceParams))
				AttachToWall(LEFT, WallRunSpeed, HitResultForward);
			else
				AttachToWall(LEFT, WallRunSpeed, HitResultLeft);
		}
		else if (GetWorld()->LineTraceSingleByChannel(HitResulRight, Start, Start + End, Channel, TraceParams))
		{
			if (GetWorld()->LineTraceSingleByChannel(HitResultForward, Start, Start + ForwardEnd, Channel, TraceParams))
				AttachToWall(RIGHT, WallRunSpeed, HitResultForward);
			else
				AttachToWall(RIGHT, WallRunSpeed, HitResulRight);
		}

	}





	DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + (GetActorForwardVector() * PlayerToWallDistance),
		FColor(255, 0, 0),
		false, -1, 0,
		12.333
	);

	DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + (GetActorRightVector() * PlayerToWallDistance),
		FColor(0, 255, 0),
		false, -1, 0,
		12.333
	);

	DrawDebugLine(
		GetWorld(),
		GetActorLocation(),
		GetActorLocation() + (-GetActorRightVector() * PlayerToWallDistance),
		FColor(0, 0, 255),
		false, -1, 0,
		12.333
	);
}

void AWallRunCharacter::AttachToWall(int Direction, float WallSpeed, FHitResult HitResult)
{
	//Stop Character
	GetCharacterMovement()->StopMovementKeepPathing();

	// Set rotation of character 90 or -90 degrees of the normal rotation.
	FRotator RotationOf90Degrees(0, Direction, 0);
	FRotator LeftOrRightDirection = RotationOf90Degrees.RotateVector(HitResult.Normal).Rotation();
	FRotator newRotation(0, LeftOrRightDirection.Yaw, 0);
	SetActorRotation(newRotation, ETeleportType::TeleportPhysics);
	// Next we are going to take the normal vector and grab its rotation and grab its right vector.
	// We times it by with wallspeed, then add the actors current location.
	FVector NewLoc = FRotationMatrix(HitResult.Normal.Rotation()).GetScaledAxis(EAxis::Y) * WallSpeed;
	if(Direction == LEFT)
		NewLoc = -NewLoc + GetActorLocation();
	else
		NewLoc = NewLoc + GetActorLocation();
	SetActorLocation(NewLoc);
}




