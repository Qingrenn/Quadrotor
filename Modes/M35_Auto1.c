#include "Modes.h"
#include "Basic.h"
#include "stdlib.h"
#include <stdio.h>
#include "M35_Auto1.h"

#include "AC_Math.h"
#include "Receiver.h"
#include "InteractiveInterface.h"
#include "ControlSystem.h"
#include "MeasurementSystem.h"

static void M35_Auto1_MainFunc();
static void M35_Auto1_enter();
static void M35_Auto1_exit();
const Mode M35_Auto1 = 
{
	50 , //mode frequency
	M35_Auto1_enter , //enter
	M35_Auto1_exit ,	//exit
	M35_Auto1_MainFunc ,	//mode main func
};

typedef struct
{
	//�˳�ģʽ������
	uint16_t exit_mode_counter;
	
	//�Զ�����״̬��
	uint8_t auto_step1;	//0-��¼��ťλ��
											//1-�ȴ���ť������� 
											//2-�ȴ������� 
											//3-�ȴ�2��
											//4-����
											//5-�ȴ��������
	uint16_t auto_counter;
	float last_button_value;
	
	float last_height;
}MODE_INF;
static MODE_INF* Mode_Inf;

static void M35_Auto1_enter()
{
	Led_setStatus( LED_status_running1 );
	
	//��ʼ��ģʽ����
	Mode_Inf = malloc( sizeof( MODE_INF ) );
	Mode_Inf->exit_mode_counter = 0;
	Mode_Inf->auto_step1 = Mode_Inf->auto_counter = 0;
	Altitude_Control_Enable();
}

static void M35_Auto1_exit()
{
	Altitude_Control_Disable();
	Attitude_Control_Disable();
	
	free( Mode_Inf );
}

static void M35_Auto1_MainFunc()
{
	const Receiver* rc = get_current_Receiver();
		
	if( rc->available == false )
	{
		//���ջ�������
		//����
		Position_Control_set_XYLock();
		Position_Control_set_TargetVelocityZ( -50 );
		return;
	}
	float throttle_stick = rc->data[0];
	float yaw_stick = rc->data[1];
	float pitch_stick = rc->data[2];
	float roll_stick = rc->data[3];	
	
	/*�ж��˳�ģʽ*/
		if( throttle_stick < 5 && yaw_stick < 5 && pitch_stick < 5 && roll_stick > 95 )
		{
			if( ++Mode_Inf->exit_mode_counter >= 50 )
			{
				change_Mode( 1 );
				return;
			}
		}
		else
			Mode_Inf->exit_mode_counter = 0;
	/*�ж��˳�ģʽ*/
		
	//�ж�ҡ���Ƿ����м�
	bool sticks_in_neutral = 
		in_symmetry_range_offset_float( throttle_stick , 5 , 50 ) && \
		in_symmetry_range_offset_float( yaw_stick , 5 , 50 ) && \
		in_symmetry_range_offset_float( pitch_stick , 5 , 50 ) && \
		in_symmetry_range_offset_float( roll_stick , 5 , 50 );
	
	extern vector3_float SDI_Point;
	extern TIME SDI_Time;
	
	if( sticks_in_neutral && get_Position_Measurement_System_Status() == Measurement_System_Status_Ready )
	{
		//ҡ�����м�
		//ִ���Զ�����		
		//ֻ����λ����Чʱ��ִ���Զ�����
		
		//��ˮƽλ�ÿ���
		Position_Control_Enable();
		switch( Mode_Inf->auto_step1 )
		{
			case 0:
				Mode_Inf->last_button_value = rc->data[5];
				++Mode_Inf->auto_step1;
				Mode_Inf->auto_counter = 0;
				break;
			
			case 1:
				//�ȴ���ť�������
				if( get_is_inFlight() == false && fabsf( rc->data[5] - Mode_Inf->last_button_value ) > 15 )
				{
					Position_Control_Takeoff_HeightRelative( 300.0f );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				else
					goto ManualControl;
				break;
				
			case 2:
				//�ȴ������ɷ�ֱ��
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					Position_Control_set_TargetPositionZRelative( 1700 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 3:
				//�ȴ������ɷ�ֱ��
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					Position_Control_set_TargetPositionXY_LatLon( 23.0424439 , 113.3884075 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 4:
				//�ȴ�ֱ�߷�������½�
				if( get_Position_ControlMode() == Position_ControlMode_Position )
				{
					Position_Control_set_TargetPositionZRelative( -1500 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
					Mode_Inf->last_height = 500;
					OLED_Draw_TickCross8x6( false , 0 , 0 );
				}
				break;
				
			case 5:
				//�ȴ����߶����
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 6:			
				OLED_Draw_Str8x6( "     " , 2 , 0 );
				OLED_Draw_Str8x6( "     " , 3 , 0 );
				if( Time_isValid(SDI_Time) && get_pass_time(SDI_Time) < 1.0f )
				{
					OLED_Draw_TickCross8x6( true , 0 , 0 );
					char str[5];
					sprintf( str , "%5.1f", SDI_Point.x );
					OLED_Draw_Str8x6( str , 2 , 0 );
					sprintf( str , "%5.1f" , SDI_Point.y );
					OLED_Draw_Str8x6( str , 3 , 0 );
					Position_Control_set_TargetVelocityBodyHeadingXY_AngleLimit( \
						constrain_float( SDI_Point.x * 0.8f , 50 ) ,	\
						constrain_float( SDI_Point.y * 0.8f , 50 ) ,	\
						0.15f ,	\
						0.15f	\
					);
					Position_Control_set_TargetVelocityZ(	\
						constrain_range_float( -SDI_Point.z * 0.1f , -35 , -50 )	\
					);
					Mode_Inf->last_height = SDI_Point.z;
				}
				else
				{
					//Ŀ�겻���ã�ɲ����λ��
					Position_Control_set_XYLock();
					if( Mode_Inf->last_height > 100 )
						Position_Control_set_ZLock();
					else
						Position_Control_set_TargetVelocityZ( -50 );
				}
				OLED_Update();
				
				//�ȴ���ť����
				if( get_is_inFlight() == false )
				{
					Position_Control_set_XYLock();
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
				
				
			case 7:
				Mode_Inf->last_button_value = rc->data[5];
				++Mode_Inf->auto_step1;
				Mode_Inf->auto_counter = 0;
				break;
			
			case 8:
				//�ȴ���ť�������
				if( get_is_inFlight() == false && fabsf( rc->data[5] - Mode_Inf->last_button_value ) > 15 )
				{
					Position_Control_Takeoff_HeightRelative( 300.0f );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				else
					goto ManualControl;
				break;
				
			case 9:
				//�ȴ������ɷ�ֱ��
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					Attitude_Control_set_Target_YawRelative( PI_f );
					Position_Control_set_TargetPositionZRelative( 1700 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 10:
				//�ȴ������ɷ�ֱ��
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					Position_Control_set_TargetPositionXYRelativeBodyHeading( 500 , 0 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 11:
				//�ȴ�ֱ�߷�������½�
				if( get_Position_ControlMode() == Position_ControlMode_Position )
				{
					Position_Control_set_TargetPositionZRelative( -1500 );
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
					Mode_Inf->last_height = 500;
				}
				break;
				
			case 12:
				//�ȴ����߶����
				if( get_Altitude_ControlMode() == Position_ControlMode_Position )
				{
					++Mode_Inf->auto_step1;
					Mode_Inf->auto_counter = 0;
				}
				break;
				
			case 13:
				OLED_Draw_TickCross8x6( false , 0 , 0 );
				OLED_Draw_Str8x6( "     " , 2 , 0 );
				OLED_Draw_Str8x6( "     " , 3 , 0 );
				if( Time_isValid(SDI_Time) && get_pass_time(SDI_Time) < 1.0f )
				{
					OLED_Draw_TickCross8x6( true , 0 , 0 );
					char str[5];
					sprintf( str , "%5.1f", SDI_Point.x );
					OLED_Draw_Str8x6( str , 2 , 0 );
					sprintf( str , "%5.1f" , SDI_Point.y );
					OLED_Draw_Str8x6( str , 3 , 0 );
					Position_Control_set_TargetVelocityBodyHeadingXY_AngleLimit( \
						constrain_float( SDI_Point.x * 0.8f , 50 ) ,	\
						constrain_float( SDI_Point.y * 0.8f , 50 ) ,	\
						0.15f ,	\
						0.15f	\
					);
					Position_Control_set_TargetVelocityZ(	\
						constrain_range_float( -SDI_Point.z * 0.1f , -35 , -50 )	\
					);
					Mode_Inf->last_height = SDI_Point.z;
				}
				else
				{
					//Ŀ�겻���ã�ɲ����λ��
					Position_Control_set_XYLock();
					if( Mode_Inf->last_height > 100 )
						Position_Control_set_ZLock();
					else
						Position_Control_set_TargetVelocityZ( -50 );
				}
				OLED_Update();
				
				//�ȴ���ť����
				if( get_is_inFlight() == false )
				{
					Position_Control_set_XYLock();
					Mode_Inf->auto_step1 = 0;
					Mode_Inf->auto_counter = 0;
				}
				break;
		}
	}
	else
	{
		ManualControl:
		//ҡ�˲����м�
		//�ֶ�����
		Mode_Inf->auto_step1 = Mode_Inf->auto_counter = 0;
		
		char str[16];
		double Lat , Lon;
		get_LatLon_From_Point( get_Position().x , get_Position().y , &Lat , &Lon );
		sprintf( str , "%d", (int)(Lat*1e7) );
		OLED_Draw_Str8x6( str , 0 , 0 );
		sprintf( str , "%d", (int)(Lon*1e7) );
		OLED_Draw_Str8x6( str , 1 , 0 );
		OLED_Update();
		
		//�ر�ˮƽλ�ÿ���
		Position_Control_Disable();
		
		//�߶ȿ�������
		if( in_symmetry_range_offset_float( throttle_stick , 5 , 50 ) )
			Position_Control_set_ZLock();
		else
			Position_Control_set_TargetVelocityZ( ( throttle_stick - 50.0f ) * 6 );

		//ƫ����������
		if( in_symmetry_range_offset_float( yaw_stick , 5 , 50 ) )
			Attitude_Control_set_YawLock();
		else
			Attitude_Control_set_Target_YawRate( ( 50.0f - yaw_stick )*0.05f );
		
		//Roll Pitch��������
		Attitude_Control_set_Target_RollPitch( \
			( roll_stick 	- 50.0f )*0.015f, \
			( pitch_stick - 50.0f )*0.015f );
	}
}